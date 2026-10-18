from pathlib import Path
import os
import subprocess

import torch
import torch.distributed as dist


def engine_commit():
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=Path(__file__).resolve().parents[1], text=True).strip()


def save_checkpoint(path, model, optimizer, config, iteration, rng, next_attempt):
    device = next(model.parameters()).device
    state = dict(torch=torch.get_rng_state(), ppo=rng.get_state(),
                 cuda=torch.cuda.get_rng_state(device) if device.type == "cuda" else None)
    states = [None] * dist.get_world_size() if dist.get_rank() == 0 else None
    dist.gather_object(state, states, dst=0)
    if dist.get_rank() != 0:
        return
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("wb") as output:
        torch.save(dict(format_version=1, model=model.state_dict(), optimizer=optimizer.state_dict(),
                        config=config, update=iteration, policy_version=iteration,
                        engine_commit=config["engine_commit"], rng_states=states, next_attempt=next_attempt), output)
        output.flush()
        os.fsync(output.fileno())
    temporary.replace(path)
