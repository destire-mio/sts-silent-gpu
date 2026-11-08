"""Prepare the immutable dev-112 seed for the Slurm 384 current-reward control."""
import argparse
from dataclasses import asdict
import hashlib
import json
from pathlib import Path

import torch

from train import OBSERVATION_VERSION
from train.e4_stage12 import CLSPolicy, MAX_LEARNING_RATE
from train.model import ModelConfig


DEV112_SHA256 = "5d6fc0ded5a10c522fc90a1844206109a205497f4c6b7c3f09ddb569828a9e45"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--init", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    torch.set_num_threads(1)
    with args.init.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    if digest != DEV112_SHA256:
        parser.error("the control requires the audited dev-112 weights")

    config = ModelConfig(384, 6, 6, 1536)
    weights = torch.load(args.init, map_location="cpu", weights_only=True)
    policy = CLSPolicy(config)
    policy.load_state_dict(weights, strict=True)
    if not all(torch.isfinite(parameter).all().item() for parameter in policy.parameters()):
        raise ValueError("dev-112 contains a nonfinite parameter")

    optimizer = torch.optim.Adam(policy.parameters(), lr=MAX_LEARNING_RATE, eps=1e-5)
    control = dict(init_checkpoint=args.init.name, init_sha256=digest, ema_decay=.998,
                   lanes=2048, rollout_length=64, cumulative_decisions=0)
    state = dict(update=0, training_seconds=0., initial_entropy=None, next_evaluation=7200.,
                 precision="encoder_fp16_heads_fp32", microbatch=256, checkpoint_serial=0, world=2,
                 stop_reason=None, high_kl_streak=0, learning_rate=MAX_LEARNING_RATE,
                 observation_version=OBSERVATION_VERSION, model_config=asdict(config),
                 claude171_value_calibration_pending=True, claude186_control=control)
    args.output.mkdir(parents=True, exist_ok=False)
    temporary = args.output / "model.pt.tmp"
    torch.save(dict(model=weights, optimizer=optimizer.state_dict(), scaler=None, ppo_rng=None, state=state),
               temporary)
    final = args.output / "model.pt"
    temporary.replace(final)
    with final.open("rb") as staged:
        checkpoint_sha256 = hashlib.file_digest(staged, "sha256").hexdigest()
    print(json.dumps(dict(checkpoint=str(final), checkpoint_sha256=checkpoint_sha256, state=state,
                          init_bytes=args.init.stat().st_size,
                          parameters=sum(parameter.numel() for parameter in policy.parameters()))))


if __name__ == "__main__":
    main()

