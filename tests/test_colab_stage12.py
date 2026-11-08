import gzip
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
    for expected in (1, 2):
        entry = tmp_path / "run_stage12.py"
        entry.write_text(f"from tools.e4_stage12 import main\nmain(device_type='cpu', stop_after_update={expected})\n")
        args = [sys.executable, "-m", "torch.distributed.run", "--standalone", "--nproc_per_node", "1",
                str(entry), "--output", str(output), "--lanes", "2", "--rollout-length", "8", "--microbatch", "16"]
        if expected == 1:
            args += ["--init", str(weights)]
        process = subprocess.run(args, env=env, text=True, capture_output=True, timeout=120)
        assert process.returncode == 0, process.stdout + process.stderr
        if expected == 2:
            assert '"event": "resumed", "rank": 0, "update": 1' in process.stdout
        saved = torch.load(output / "model.pt", map_location="cpu", weights_only=True)
        assert saved["state"]["update"] == expected
        assert {int(x["step"]) for x in saved["optimizer"]["state"].values()} == {2 * expected}
        assert saved["state"]["checkpoint_serial"] == expected + 1
