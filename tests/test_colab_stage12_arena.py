import gzip
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest
import torch
from torch import nn

from train.e4_stage12_ema import ExponentialWeights


def test_ema_updates_and_resumes_without_changing_live_model():
    model = nn.Linear(1, 1, bias=False)
    with torch.no_grad():
        model.weight.fill_(2.)
    ema = ExponentialWeights(model, .75)
    with torch.no_grad():
        model.weight.fill_(6.)
    ema.update()
    assert ema(torch.ones(1, 1)).item() == 3.
    assert model(torch.ones(1, 1)).item() == 6.
    resumed = ExponentialWeights(model, .75, ema.weights)
    with torch.no_grad():
        model.weight.fill_(7.)
    resumed.update()
    assert resumed(torch.ones(1, 1)).item() == 4.
    assert ema(torch.ones(1, 1)).item() == 3.


def test_arena_dev132_update_checkpoint_resume(tmp_path):
    source = os.environ.get("E4_TEST_DEV132")
    if source is None:
        pytest.skip("set E4_TEST_DEV132 to the actual 768-wide checkpoint")
    output = tmp_path / "training"
    output.mkdir()
    with gzip.open(output / "baseline.json.gz", "wt") as stream:
        stream.write("[]")
    env = dict(os.environ, CUDA_VISIBLE_DEVICES="", OMP_NUM_THREADS="1", MKL_NUM_THREADS="1")
    env.pop("SLURM_JOB_ID", None)
    previous_ema = None
    for update in (1, 2):
        entry = tmp_path / "run_arena.py"
        entry.write_text(
            """from pathlib import Path
import os
import torch
import train.e4_stage12_arena as arena
from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig
from tools.e4_stage12 import main

class SeededArena(arena.ArenaCollector):
    def __init__(self, search, rng, **options):
        super().__init__(search, rng, **options)
        if Path(os.environ["E4_TEST_ARENA_OUTPUT"], "model.pt").exists():
            return
        # Warm exactly one real practice case from a known dev-132 Act 2 Boss seed.
        # The same capture callback is used by normal training.
        policy = CLSPolicy(ModelConfig(768, 12, 12, 3072)).eval()
        policy.load_state_dict(torch.load(os.environ["E4_TEST_DEV132"], map_location="cpu", weights_only=True))
        self.reset([0], [2000000572])
        search.greedy = True
        with torch.no_grad():
            for _ in range(3500):
                progress = search.progress_for([0])
                action, _, _, _ = self.choose(policy, [0], progress)
                if self.arena_pools["act2_boss"]:
                    break
                reward, terminal = self.advance([0], action, progress)
                self.after_transition(policy, [0], progress, reward, terminal, None)
                assert not terminal[0], self.completed
            else:
                raise AssertionError("fixture seed did not reach Act 2 Boss")
        search.greedy = False
        self.seeds[0] = None
        self.decisions = 0
        self.completed.clear()

arena.ArenaCollector = SeededArena
""" + f"\nmain(device_type='cpu', stop_after_update={update})\n",
            encoding="utf-8",
        )
        env["E4_TEST_ARENA_OUTPUT"] = str(output)
        args = [sys.executable, "-m", "torch.distributed.run", "--standalone", "--nproc_per_node=1", str(entry),
                "--output", str(output), "--lanes", "2", "--rollout-length", "32", "--microbatch", "16"]
        if update == 1:
            args += ["--init", source, "--arena-fraction", ".5", "--ema-decay", ".998",
                     "--value-coefficient", "76.96145098000254"]
        run = subprocess.run(args, env=env, text=True, capture_output=True, timeout=360)
        assert run.returncode == 0, run.stdout + run.stderr
        events = [json.loads(line) for line in run.stdout.splitlines() if line.startswith("{")]
        if update == 2:
            assert any(e.get("event") == "resumed" and e["update"] == 1 for e in events)
        row, = [e for e in events if e.get("event") == "update"]
        assert row["finite"] and row["optimizer_steps"] == 2
        assert row["arena"]["act2_boss"]["pool_size"] >= 1
        if update == 1:
            assert sum(v["starts"] for v in row["arena"].values()) >= 1
        saved = torch.load(output / "model.pt", map_location="cpu", weights_only=True, mmap=True)
        assert saved["state"]["update"] == update
        assert saved["state"]["standalone_arena_fraction"] == .5
        assert saved["state"]["standalone_ema_decay"] == .998
        assert saved["model"].keys() == saved["ema_weights"].keys()
        avg = saved["ema_weights"]["cls"].clone()
        assert not torch.equal(saved["model"]["cls"], avg)
        if previous_ema is not None:
            assert not torch.equal(previous_ema, avg)
        previous_ema = avg
        ranked = torch.load(output / "rank-0.pt", map_location="cpu", weights_only=False)
        arena_state = ranked["collector"]["arena"]
        assert len(arena_state["pools"]["act2_boss"]) >= 1
        starts = ranked["collector"]["midstart"]["starts"]
        for slot, kind in enumerate(starts):
            if kind.startswith("arena_"):
                assert ranked["collector"]["midstart"]["randomizations"][slot]
        print(json.dumps(dict(update=update, optimizer_steps=row["optimizer_steps"], arena=row["arena"])))




def test_arena_win_bootstraps_and_death_has_zero_reward():
    from collections import Counter
    from types import SimpleNamespace

    from train.e4_stage12_arena import ArenaCollector

    collector = ArenaCollector.__new__(ArenaCollector)
    collector.arena_fights = {0: "act2_boss", 1: "act3_boss"}
    collector.arena_stats = {"act2_boss": Counter(), "act3_boss": Counter()}
    collector.stalled = set()
    collector.seeds = [1, None]
    collector.completed = [{"outcome": "death"}]
    collector.search = SimpleNamespace(
        last_step_progress={0: [33, 2, 2, 2, 6, 0, 1, 0, 24],
                            1: [50, 3, 1, 2, 4, 0, 1, 0, 0]},
        scores=lambda model, slots, progress: {slot: .25 for slot in slots},
    )
    rewards, terminal = [5 / 156, .5], [False, True]
    collector.after_transition(None, [0, 1], {0: [33, 2, 1, 2, 0], 1: [50, 3, 1, 2, 0]},
                               rewards, terminal, None)
    assert rewards == [5 / 156 + .25, 0.]
    assert terminal == [True, True]
    assert collector.seeds == [None, None]
    assert not collector.completed and not collector.arena_fights
    assert collector.arena_stats["act2_boss"] == {"wins": 1, "games": 1}
    assert collector.arena_stats["act3_boss"] == {"wins": 0, "games": 1}
