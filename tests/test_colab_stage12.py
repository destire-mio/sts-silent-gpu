import gzip
import json
import os
import subprocess
import sys

import torch

from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig


def test_colab_stage12_cpu_restarts_from_checkpoint(tmp_path):
    weights = tmp_path / "init.pt"
    torch.manual_seed(1729)
    torch.save(CLSPolicy(ModelConfig(384, 6, 6, 1536)).state_dict(), weights)
    output = tmp_path / "training"
    output.mkdir()
    with gzip.open(output / "baseline.json.gz", "wt") as stream:
        stream.write("[]")
    env = dict(os.environ, CUDA_VISIBLE_DEVICES="", OMP_NUM_THREADS="1", MKL_NUM_THREADS="1")
    env.pop("SLURM_JOB_ID", None)
    coefficient = 76.96145098000254
    diagnostic = output / "claude133-gradient-diagnostic.json"
    for expected in (1, 2, 3):
        entry = tmp_path / "run_stage12.py"
        entry.write_text(f"from tools.e4_stage12 import main\nmain(device_type='cpu', stop_after_update={expected})\n")
        args = [sys.executable, "-m", "torch.distributed.run", "--standalone", "--nproc_per_node", "1",
                str(entry), "--output", str(output), "--lanes", "2", "--rollout-length", "8", "--microbatch", "16"]
        if expected == 1:
            args += ["--init", str(weights)]
        if expected == 2:
            args += ["--value-coefficient", str(coefficient)]
        process = subprocess.run(args, env=env, text=True, capture_output=True, timeout=120)
        assert process.returncode == 0, process.stdout + process.stderr
        if expected > 1:
            assert f'"event": "resumed", "rank": 0, "update": {expected - 1}' in process.stdout
        saved = torch.load(output / "model.pt", map_location="cpu", weights_only=True)
        assert saved["state"]["update"] == expected
        assert {int(x["step"]) for x in saved["optimizer"]["state"].values()} == {2 * expected}
        assert saved["state"]["checkpoint_serial"] == expected + 1
        if expected == 1:
            assert diagnostic.is_file()
            # Simulate a checkpoint saved before automatic calibration.
            del saved["state"]["value_calibration"]
            torch.save(saved, output / "model.pt")
            diagnostic.unlink()
        else:
            assert saved["state"]["standalone_value_coefficient"] == coefficient
            assert "value_calibration" not in saved["state"]
            assert not diagnostic.exists()
            updates = [json.loads(line) for line in process.stdout.splitlines()
                       if line.startswith('{"event": "update",')]
            assert len(updates) == 1
            assert updates[0]["value_loss_coefficient"] == coefficient
            assert updates[0]["finite"]



def test_colab_stage12_384_defers_nonfinite_startup_calibration(tmp_path):
    from tools.e4_stage12 import MAX_LEARNING_RATE

    weights = tmp_path / "init.pt"
    torch.manual_seed(1729)
    torch.save(CLSPolicy(ModelConfig(384, 6, 6, 1536)).state_dict(), weights)
    output = tmp_path / "training"
    output.mkdir()
    with gzip.open(output / "baseline.json.gz", "wt") as stream:
        stream.write("[]")
    entry = tmp_path / "run_stage12.py"
    entry.write_text(
        """import train.e4_stage12 as learner
from tools.e4_stage12 import main


def zero_actor_gradient(*args, **kwargs):
    return dict(actor_encoder_norm=0., weighted_value_encoder_norm=1.,
                ratio=float("inf"), target=.5)


learner.gradient_measurement = zero_actor_gradient
main(device_type="cpu", stop_after_update=1)
""",
        encoding="utf-8",
    )
    env = dict(os.environ, CUDA_VISIBLE_DEVICES="", OMP_NUM_THREADS="1", MKL_NUM_THREADS="1")
    env.pop("SLURM_JOB_ID", None)
    args = [sys.executable, "-m", "torch.distributed.run", "--standalone", "--nproc_per_node=1",
            str(entry), "--init", str(weights), "--output", str(output),
            "--lanes", "2", "--rollout-length", "8", "--microbatch", "16"]
    process = subprocess.run(args, env=env, text=True, capture_output=True, timeout=120)
    assert process.returncode == 0, process.stdout + process.stderr
    events = [json.loads(line) for line in process.stdout.splitlines() if line.startswith("{")]
    deferred, = [row for row in events if row.get("event") == "claude171_value_calibration_deferred"]
    assert deferred["update"] == 0 and deferred["actor_encoder_norm"] == 0.
    update, = [row for row in events if row.get("event") == "update"]
    assert update["update"] == 1 and update["phase"] == "PPO" and update["finite"]
    assert update["learning_rate"] == MAX_LEARNING_RATE
    assert not (output / "claude133-gradient-diagnostic.json").exists()
