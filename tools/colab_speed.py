"""Time Stage12 rollout and PPO on one CUDA device."""
import argparse
from contextlib import redirect_stdout
import json
from pathlib import Path
import sys
import time

import torch

from train.e4_midstart import MidstartCollector
from train.e4_stage12 import CLSPolicy, MAX_LEARNING_RATE, NoSLSearch, local_minibatch
from train.model import ModelConfig, packed_inputs
from train.ppo import PPOConfig, objective
from train.rollout import synchronize


ROLLOUT_LENGTH = 64
MINIBATCH = 32768
CONFIGS = {
    384: ModelConfig(384, 6, 6, 1536),
    768: ModelConfig(768, 12, 12, 3072),
}
DTYPES = {"fp16": torch.float16, "bf16": torch.bfloat16}


class SpeedPolicy(CLSPolicy):
    def __init__(self, config, dtype):
        super().__init__(config)
        self.encoder_dtype_override = dtype


def learn(policy, optimizer, scaler, fragment, config, rng):
    indices = fragment["action_valid"].nonzero().flatten()
    assert len(indices), "No valid PPO samples"
    advantages = fragment["advantages"][indices].double()
    mean = advantages.mean()
    std = (advantages.square().mean() - mean.square()).clamp_min(0).sqrt()
    fragment["advantages"] = ((fragment["advantages"] - mean) / (std + 1e-8)).float()
    cursor = dict(fragment=fragment, indices=indices, counts=[len(indices)], global_minibatch=MINIBATCH)
    for _ in range(config.epochs):
        cursor["order"] = torch.randperm(len(indices), device=indices.device, generator=rng)
        for start in range(0, len(indices), MINIBATCH):
            cursor["start"] = start
            selected, count = local_minibatch(cursor, rank=0)
            optimizer.zero_grad(set_to_none=True)
            for ids in selected.split(config.microbatch_size):
                inputs = packed_inputs(fragment["data"], fragment["offsets"], ids, ids.device)
                legal = inputs["candidate_mask"]
                legal &= fragment["policy_mask"][ids, :legal.shape[1]]
                logits, values = policy(inputs)
                loss, _ = objective(logits.float(), values, legal, fragment["actions"][ids],
                                    fragment["logs"][ids], fragment["returns"][ids],
                                    fragment["advantages"][ids], config)
                scaler.scale(loss * (len(ids) / count)).backward()
            scaler.unscale_(optimizer)
            torch.nn.utils.clip_grad_norm_(policy.parameters(), config.max_grad_norm,
                                           error_if_nonfinite=not scaler.is_enabled())
            scaler.step(optimizer)
            scaler.update()


def run(args, device):
    torch.manual_seed(1729)
    policy = SpeedPolicy(CONFIGS[args.width], DTYPES[args.dtype])
    if args.checkpoint is not None:
        policy.load_state_dict(torch.load(args.checkpoint, map_location="cpu", weights_only=True), strict=True)
    policy.to(device)
    config = PPOConfig(learning_rate=MAX_LEARNING_RATE, epochs=2, minibatch_size=MINIBATCH,
                       microbatch_size=args.microbatch)
    optimizer = torch.optim.Adam(policy.parameters(), lr=config.learning_rate, eps=1e-5)
    cuda = device.type == "cuda"
    scaler = torch.amp.GradScaler(device.type, enabled=cuda and args.dtype == "fp16")
    rng = torch.Generator(device=device).manual_seed(1730)
    search = NoSLSearch(args.lanes, device, 1731, storage_slots=1024)
    collector = MidstartCollector(search, torch.Generator().manual_seed(1732))
    for round_number in range(1, args.rounds + 1):
        policy.eval()
        search.fault_counts.clear()
        previous_decisions = collector.decisions
        synchronize(device)
        if cuda:
            torch.cuda.reset_peak_memory_stats(device)
        started = time.perf_counter()
        with redirect_stdout(sys.stderr):
            fragment = collector.collect(policy, length=ROLLOUT_LENGTH)
        synchronize(device)
        sampled = time.perf_counter()
        # Stage12 has no dropout. Eval mode with gradients keeps the same PPO
        # computation and bypasses CLSPolicy's training-only checkpointing.
        policy.train(args.recompute == "on")
        learn(policy, optimizer, scaler, fragment, config, rng)
        synchronize(device)
        finished = time.perf_counter()
        decisions = collector.decisions - previous_decisions
        yield dict(round=round_number, rollout_seconds=sampled - started, learner_seconds=finished - sampled,
                   decisions=decisions, decisions_per_second=decisions / (finished - started),
                   rollout_decisions_per_second=decisions / (sampled - started),
                   peak_allocated_bytes=torch.cuda.max_memory_allocated(device) if cuda else 0,
                   peak_reserved_bytes=torch.cuda.max_memory_reserved(device) if cuda else 0,
                   faults=sum(search.fault_counts.values()))
        del fragment


def positive_int(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint", type=Path, help="Weights-only Stage12 snapshot")
    parser.add_argument("--width", type=int, choices=CONFIGS, required=True)
    parser.add_argument("--lanes", type=positive_int, default=2048)
    parser.add_argument("--microbatch", type=positive_int, default=256)
    parser.add_argument("--recompute", choices=("on", "off"), default="on")
    parser.add_argument("--dtype", choices=DTYPES, default="fp16")
    parser.add_argument("--rounds", type=positive_int, default=3)
    return parser.parse_args(argv)


def main():
    args = parse_args()
    torch.set_num_threads(1)
    device = torch.device("cuda:0")
    torch.cuda.set_device(device)
    for row in run(args, device):
        print(json.dumps(row, allow_nan=False), flush=True)


if __name__ == "__main__":
    main()
