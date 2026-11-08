"""Compare encoder checkpoint policies during the first six 768-wide rounds."""
from statistics import mean

import torch
import torch.distributed as dist

from train.model import record_lengths

TRIAL_ROUNDS = 6
SAMPLES_PER_MODE = 2
MEMORY_RESERVE = 2 * 1024**3


def trial_mode(history, world, update):
    rows = [row for row in history if row["world"] == world and not row["resumed"]]
    full = [row["learner_seconds"] for row in rows if row["mode"] == "full"]
    reduced = [row["learner_seconds"] for row in rows if row["mode"] == "reduced"]
    if len(full) >= SAMPLES_PER_MODE and len(reduced) >= SAMPLES_PER_MODE:
        winner = "reduced" if mean(reduced[:2]) < mean(full[:2]) else "full"
        return winner, winner
    if update >= TRIAL_ROUNDS:
        return "full", "full"
    return ("reduced" if len(reduced) < len(full) else "full"), None


def extra_activation_bytes(batch, entities, config):
    tokens = entities + 1
    # Two layers run directly in reduced mode. Budget FP32 storage for
    # intermediates and attention matrices even when the encoder uses AMP.
    hidden = tokens * (24 * config.width + 8 * config.feedforward)
    attention = 4 * config.heads * tokens * tokens
    return 2 * batch * 4 * (hidden + attention)


def prepare_round(policy, cursor, state, world, resumed):
    fragment = cursor["fragment"]
    indices = cursor["indices"]
    device = indices.device
    if len(indices):
        entities, candidates = record_lengths(fragment["data"], fragment["offsets"], indices)
        entities, candidates = int(entities.max()), int(candidates.max())
    else:
        entities = candidates = 0
    microbatch = state["microbatch"]
    history = state.get("claude165_recompute", [])
    requested, winner = ("full", "full") if state.get("claude167_retry") else trial_mode(history, world, state["update"])
    budget = 0
    if requested == "reduced":
        free, _ = torch.cuda.mem_get_info(device)
        previous_peaks = [row["peak_reserved_bytes"] for row in history
                          if row["world"] == world and row["mode"] == "full"]
        base_growth = max(0, max(previous_peaks, default=0) - torch.cuda.memory_reserved(device))
        budget = free - MEMORY_RESERVE - base_growth - extra_activation_bytes(microbatch, entities, policy.config)
    envelope = torch.tensor([entities, candidates, -budget], device=device, dtype=torch.int64)
    dist.all_reduce(envelope, op=dist.ReduceOp.MAX)
    entities, candidates, negative_budget = envelope.tolist()
    mode = "reduced" if requested == "reduced" and negative_budget <= 0 else "full"
    policy.encoder_checkpoint_all = mode == "full"
    return dict(mode=mode, requested=requested, selected=winner, resumed=resumed, world=world,
                microbatch=microbatch, max_entities=entities, max_candidates=candidates,
                min_headroom_bytes=-negative_budget if requested == "reduced" else None)


def finish_round(record, device, learner_seconds, seconds):
    peaks = torch.tensor([torch.cuda.max_memory_allocated(device), torch.cuda.max_memory_reserved(device)],
                         device=device, dtype=torch.int64)
    dist.all_reduce(peaks, op=dist.ReduceOp.MAX)
    allocated, reserved = peaks.tolist()
    return dict(record, learner_seconds=learner_seconds, round_seconds=seconds,
                peak_allocated_bytes=allocated, peak_reserved_bytes=reserved)
