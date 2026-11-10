from copy import deepcopy
import json
import time

import torch

from tools.e4_value_truth import collector, exam_comparison, remaining_returns, RootCollector


class EndTurnPolicy(torch.nn.Module):
    def forward(self, inputs):
        kinds = inputs["candidate_kinds"]
        logits = torch.where(kinds == 2, 100., 0.).masked_fill(~inputs["candidate_mask"], -torch.inf)
        return logits, torch.full((len(kinds),), .25)


def test_value_truth_cpu_smoke():
    torch.set_num_threads(1)
    model = EndTurnPolicy()
    runner = collector(2, torch.device("cpu"), 101, time.monotonic() + 90, storage_slots=1,
                       cls=RootCollector)
    seeds = [2_000_000_000, 2_000_000_001]
    runner.reset([0], seeds[:1])
    root_slot, = runner.search.take(1)
    runner.search.clone([0], [root_slot])
    progress = runner.search.progress_for([root_slot])[root_slot]
    root = dict(slot=root_slot, seed=seeds[0], progress=progress, ledger=deepcopy(runner.rewards[0]))

    runner.search.greedy = True
    argmax = runner.evaluate(model, seeds)
    runner.search.greedy = False
    sampled = runner.evaluate(model, seeds)
    exam = exam_comparison(sampled, argmax)
    nodes, decisions = remaining_returns(runner, model, {"start": [root]}, 2, 221)
    row = nodes["start"]
    assert exam["episodes"] == 2 and [r["seed"] for r in exam["seeds"]] == seeds
    assert row["complete_roots"] == 1 and row["complete_futures"] == 2
    assert row["mean_value"] == .25 and 0 < row["mean_return"] <= 1
    assert row["bias"] == .25 - row["mean_return"]
    assert len({trial["seed"] for trial in row["states"][0]["futures"]}) == 2
    assert decisions["combat"]["samples"] == 2
    assert runner.search.progress_for([root_slot])[root_slot] == progress
    json.dumps([exam, nodes, decisions], allow_nan=False)
