from pathlib import Path
import subprocess

import torch


def engine_commit():
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=Path(__file__).resolve().parents[1], text=True).strip()


def save_checkpoint(path, model, optimizer, config, iteration):
    torch.save(dict(model=model.state_dict(), optimizer=optimizer.state_dict(), config=config, update=iteration,
                    policy_version=iteration, engine_commit=config["engine_commit"], torch_rng=torch.get_rng_state()), path)
