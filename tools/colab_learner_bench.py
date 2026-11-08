"""Measure one Stage12 PPO optimizer step on the same collected observations."""
import argparse
import json
from pathlib import Path
import tempfile
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.colab_learner import configure_learner
from train.e4_midstart import MidstartCollector
from train.e4_stage12 import (
    CLSPolicy, MAX_LEARNING_RATE, NoSLSearch, advance_minibatch, local_minibatch, new_cursor,
)
from train.model import packed_inputs
from train.rollout import synchronize
from tools.colab_speed import CONFIGS, DTYPES, positive_int


TOLERANCE = 1e-3


def attention_backend(policy, cursor, microbatch, device):
    if device.type == "cpu":
        return ["cpu"]
    selected, _ = local_minibatch(cursor, 0)
    fragment = cursor["fragment"]
    ids = selected[:microbatch]
    inputs = packed_inputs(fragment["data"], fragment["offsets"], ids, device)
    inputs["candidate_mask"] &= fragment["policy_mask"][ids, :inputs["candidate_mask"].shape[1]]
    with torch.profiler.profile(activities=[torch.profiler.ProfilerActivity.CPU]) as profile:
        policy(inputs)
        synchronize(device)
    events = {event.key for event in profile.key_averages()}
    backends = [name for name, pattern in (
        ("flash", "_scaled_dot_product_flash_attention"),
        ("efficient", "_scaled_dot_product_efficient_attention"),
        ("math", "_scaled_dot_product_attention_math"),
        ("cudnn", "_scaled_dot_product_cudnn_attention"),
    ) if any(pattern in event for event in events)]
    return backends or ["unobserved"]


def compare(args, device, variants):
    cuda = device.type == "cuda"
    config = CONFIGS[args.width]
    torch.manual_seed(args.seed)
    policy = CLSPolicy(config)
    if args.checkpoint:
        policy.load_state_dict(torch.load(args.checkpoint, map_location="cpu", weights_only=True), strict=True)
    initial = {key: value.detach().clone() for key, value in policy.state_dict().items()}
    policy.encoder_dtype_override = DTYPES[args.dtype] if cuda else None
    policy.to(device).eval()
    search = NoSLSearch(args.lanes, device, args.seed + 1, storage_slots=1024)
    collector = MidstartCollector(search, torch.Generator().manual_seed(args.seed + 2))
    with torch.no_grad():
        fragment = collector.collect(policy, length=args.rollout_length)
    search.midstart_collector = None
    del collector, search, policy
    results = []
    for compile_encoder, fused_sdpa in variants:
        policy = CLSPolicy(config)
        policy.load_state_dict(initial, strict=True)
        policy.encoder_dtype_override = DTYPES[args.dtype] if cuda else None
        policy.encoder_checkpoint_enabled = args.recompute == "on"
        policy.to(device).train()
        fallbacks = configure_learner(policy, compile_encoder=compile_encoder, fused_sdpa=fused_sdpa)
        model = DistributedDataParallel(policy, device_ids=[device.index] if cuda else None, broadcast_buffers=False)
        optimizer = torch.optim.Adam(model.parameters(), lr=MAX_LEARNING_RATE, eps=1e-5)
        scaler = torch.amp.GradScaler(device.type, enabled=cuda and args.dtype == "fp16")
        rng = torch.Generator(device=device).manual_seed(args.seed + 3)
        cursor = new_cursor(dict(fragment), rng, 1)
        if not cursor["counts"][0]:
            raise RuntimeError("The collector produced no valid PPO observations")
        synchronize(device)
        start = time.perf_counter()
        advance_minibatch(model, optimizer, scaler, cursor, rng, 0, 1, args.microbatch)
        synchronize(device)
        seconds = time.perf_counter() - start
        totals = cursor["totals"]
        result = dict(compile=compile_encoder, fused_sdpa=fused_sdpa, seconds=seconds,
                      valid_examples=int(totals[6]), loss=float(totals[5] / totals[6]),
                      gradient_norm=cursor["gradient_stats"][0],
                      amp_skips=cursor["amp_skipped_steps"])
        probe_rng = torch.Generator(device=device).manual_seed(args.seed + 3)
        result["attention_backend"] = attention_backend(
            policy, new_cursor(dict(fragment), probe_rng, 1), args.microbatch, device)
        result["fused_fallbacks"] = sorted(fallbacks)
        if fused_sdpa and result["attention_backend"] == ["unobserved"] and not fallbacks:
            result["attention_constraint"] = "Math disabled for the full learner step. Compiled kernel name unavailable."
        baseline = results[0] if results else result
        for key in ("loss", "gradient_norm"):
            delta = abs(result[key] - baseline[key]) / max(abs(baseline[key]), 1e-12)
            result[key + "_relative_difference"] = delta
        result["matches_baseline"] = (
            result["valid_examples"] == baseline["valid_examples"]
            and not result["amp_skips"]
            and result["loss_relative_difference"] < TOLERANCE
            and result["gradient_norm_relative_difference"] < TOLERANCE
        )
        results.append(result)
        del model, optimizer, policy
    return results


def run(args, device, variants=((False, False), (True, False), (False, True), (True, True))):
    if device.type == "cuda":
        torch.cuda.set_device(device)
    with tempfile.TemporaryDirectory() as directory:
        backend = "nccl" if device.type == "cuda" else "gloo"
        dist.init_process_group(backend, init_method=(Path(directory) / "pg").as_uri(), rank=0, world_size=1)
        try:
            return compare(args, device, variants)
        finally:
            dist.destroy_process_group()


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint", type=Path)
    parser.add_argument("--width", type=int, choices=CONFIGS, default=768)
    parser.add_argument("--lanes", type=positive_int, default=512)
    parser.add_argument("--rollout-length", type=positive_int, default=64)
    parser.add_argument("--microbatch", type=positive_int, default=512)
    parser.add_argument("--dtype", choices=DTYPES, default="fp16")
    parser.add_argument("--recompute", choices=("on", "off"), default="off")
    parser.add_argument("--seed", type=int, default=1729)
    return parser.parse_args(argv)


def main():
    torch.set_num_threads(1)
    results = run(parse_args(), torch.device("cuda:0"))
    for result in results:
        print(json.dumps(result), flush=True)
    return 0 if all(row["matches_baseline"] for row in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
