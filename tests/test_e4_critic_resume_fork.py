import gzip
import json
import os
import shutil
import subprocess
import sys

import torch

from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig


def test_enable_critic_on_resume_matches_first_policy_and_value_forward(tmp_path):
    init = tmp_path / "init.pt"
    torch.manual_seed(1729)
    torch.save(CLSPolicy(ModelConfig(384, 6, 6, 1536)).state_dict(), init)
    mother = tmp_path / "mother"
    mother.mkdir()
    with gzip.open(mother / "baseline.json.gz", "wt") as stream:
        stream.write("[]")
    entry = tmp_path / "run.py"
    entry.write_text(
        """import sys
from pathlib import Path
import torch
from tools import e4_stage12 as app

target = Path(sys.argv.pop(1))
limit = int(sys.argv.pop(1))
original = app.NoSLSearch.predict


def capture_first(search, model, slots):
    if search.greedy or target.exists():
        return original(search, model, slots)
    captured = {}

    def save_forward(_module, _inputs, output):
        captured["logits"], captured["policy_value"] = [tensor.detach().cpu() for tensor in output]

    hook = model.register_forward_hook(save_forward)
    try:
        actions, values, logs = original(search, model, slots)
    finally:
        hook.remove()
    if captured:
        torch.save(dict(captured, actions=actions, rollout_values=values, logs=logs), target)
    return actions, values, logs


app.NoSLSearch.predict = capture_first
app.main(device_type="cpu", stop_after_update=limit)
""",
        encoding="utf-8",
    )
    env = dict(os.environ, CUDA_VISIBLE_DEVICES="", OMP_NUM_THREADS="1", MKL_NUM_THREADS="1")
    env.pop("SLURM_JOB_ID", None)

    def run(folder, limit, extra):
        args = [sys.executable, "-m", "torch.distributed.run", "--standalone", "--nproc_per_node", "1",
                str(entry), str(folder / "first-forward.pt"), str(limit), "--output", str(folder),
                "--lanes", "2", "--rollout-length", "8", "--microbatch", "16", *extra]
        proc = subprocess.run(args, env=env, capture_output=True, text=True, timeout=100)
        assert proc.returncode == 0, proc.stdout + proc.stderr
        return [json.loads(line) for line in proc.stdout.splitlines() if line.startswith("{")]

    run(mother, 1, ["--init", str(init), "--ema-decay", ".998", "--value-coefficient", ".5"])
    (mother / "first-forward.pt").unlink()
    disabled, enabled = tmp_path / "disabled", tmp_path / "enabled"
    shutil.copytree(mother, disabled)
    shutil.copytree(mother, enabled)
    run(disabled, 2, [])
    events = run(enabled, 2, ["--separate-critic"])
    created, = [row for row in events if row.get("event") == "critic_created_on_resume"]
    assert created["update"] == 1
    off = torch.load(disabled / "first-forward.pt", weights_only=True)
    on = torch.load(enabled / "first-forward.pt", weights_only=True)
    assert off.keys() == on.keys()
    for key, value in off.items():
        assert torch.equal(value, on[key]) if torch.is_tensor(value) else value == on[key]
    saved = torch.load(enabled / "model.pt", map_location="cpu", weights_only=True)
    assert saved["state"]["update"] == 2
    policy_steps = {int(item["step"]) for item in saved["optimizer"]["state"].values()}
    critic_steps = {int(item["step"]) for item in saved["critic"]["optimizer"]["state"].values()}
    assert policy_steps == {2, 4} and critic_steps == {2}
    assert "ema_weights" in saved and "ema_weights" in saved["critic"]
