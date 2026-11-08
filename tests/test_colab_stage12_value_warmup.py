import gzip
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest
import torch

from train.e4 import BOSS_ROOM, COMBAT_PHASE, COMPLETE_PHASE, ELITE_ROOM, MilestoneRewardLedger


def test_milestone_rewards_reuse_the_existing_ledger():
    def state(floor, act, phase=COMPLETE_PHASE, room=0, won=False):
        return [floor, act, phase, room, 9, 0, int(won), 0, 30]

    ledger = MilestoneRewardLedger()
    assert ledger.advance(state(0, 1), state(1, 1)) == pytest.approx(.001)
    for act, floor, reward in ((1, 16, .01), (2, 33, .03), (3, 50, .04), (3, 51, .04)):
        before = state(floor, act, COMBAT_PHASE, BOSS_ROOM)
        after = state(floor, act, room=BOSS_ROOM, won=True)
        assert ledger.advance(before, after) == pytest.approx(reward)
        assert ledger.advance(after, after) == 0.
    assert ledger.advance(state(51, 3), state(52, 4)) == pytest.approx(.061)
    assert ledger.advance(state(54, 4, COMBAT_PHASE, ELITE_ROOM),
                          state(54, 4, room=ELITE_ROOM, won=True)) == pytest.approx(.05)
    assert ledger.advance(state(55, 4, COMBAT_PHASE, BOSS_ROOM),
                          state(55, 4, room=BOSS_ROOM, won=True)) == pytest.approx(.70)
    assert all(ledger.reach[key] for key in ("act1_boss", "act2_boss", "act3_first_boss",
                                          "act3_second_boss", "act4", "shield_spear", "heart"))


def test_dev132_milestone_warmup_resume_and_recalibration(tmp_path):
    source = os.environ.get("E4_TEST_DEV132")
    if source is None:
        pytest.skip("set E4_TEST_DEV132 to the actual 768-wide checkpoint")
    output = tmp_path / "training"
    output.mkdir()
    with gzip.open(output / "baseline.json.gz", "wt") as stream:
        stream.write("[]")
    env = dict(os.environ, CUDA_VISIBLE_DEVICES="", OMP_NUM_THREADS="1", MKL_NUM_THREADS="1")
    env.pop("SLURM_JOB_ID", None)
    initial = torch.load(source, map_location="cpu", weights_only=True, mmap=True)
    for update in (1, 2, 3):
        entry = tmp_path / "run_warmup.py"
        entry.write_text(f"from tools.e4_stage12 import main\nmain(device_type='cpu', stop_after_update={update})\n",
                         encoding="utf-8")
        args = [sys.executable, "-m", "torch.distributed.run", "--standalone", "--nproc_per_node=1", str(entry),
                "--output", str(output), "--lanes", "2", "--rollout-length", "24", "--microbatch", "16"]
        if update == 1:
            args += ["--init", source, "--reward-scheme", "milestone", "--value-warmup-updates", "2"]
        process = subprocess.run(args, env=env, text=True, capture_output=True, timeout=260)
        assert process.returncode == 0, process.stdout + process.stderr
        events = [json.loads(line) for line in process.stdout.splitlines() if line.startswith("{")]
        if update > 1:
            assert any(e.get("event") == "resumed" and e["update"] == update - 1 for e in events)
        row, = [e for e in events if e.get("event") == "update"]
        assert row["finite"] and row["optimizer_steps"] == 2 and row["reward_scheme"] == "milestone"
        saved = torch.load(output / "model.pt", map_location="cpu", weights_only=True, mmap=True)
        assert saved["state"]["standalone_reward_scheme"] == "milestone"
        assert saved["state"]["standalone_value_warmup"] == {"updates": 2, "completed": min(update, 2)}
        rank = torch.load(output / "rank-0.pt", map_location="cpu", weights_only=False)
        assert all(isinstance(ledger, MilestoneRewardLedger) for ledger in rank["collector"]["rewards"])
        if update < 3:
            assert row["phase"] == "value_warmup" and row["value_warmup"]["round"] == update
            assert sum(a["samples"] for a in row["value_warmup"]["by_act"].values()) == row["samples"]
            assert row["value_warmup"]["by_act"]["1"]["mse"] == pytest.approx(row["value_loss"])
            assert all(torch.equal(value, initial[name]) for name, value in saved["model"].items()
                       if not name.startswith("value."))
            assert any(not torch.equal(value, initial[name]) for name, value in saved["model"].items()
                       if name.startswith("value."))
            assert len(saved["optimizer"]["state"]) == len([k for k in initial if k.startswith("value.")])
            assert {int(v["step"]) for v in saved["optimizer"]["state"].values()} == {2 * update}
            assert not any(e.get("event") == "claude171_value_recalibrated" for e in events)
        else:
            assert row["phase"] == "PPO"
            measured, = [e for e in events if e.get("event") == "claude171_value_recalibrated"]
            assert measured["start_update"] == 3
            assert measured["ratio"] > 0
            assert not torch.equal(saved["model"]["cls"], initial["cls"])
            assert {int(v["step"]) for v in saved["optimizer"]["state"].values()} == {2, 6}
        print(json.dumps({key: row[key] for key in ("update", "phase", "value_loss", "optimizer_steps")}
                         | {"value_warmup": row.get("value_warmup")}))
