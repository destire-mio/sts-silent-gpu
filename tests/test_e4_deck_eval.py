import gzip
import json
from pathlib import Path
import random

import pytest
import torch

from tools.e4_deck_eval import DeckEvaluator, backend, choose, fit, label_roots, write_collection
from tools.e4_value_truth import RootCollector
from train.e4_stage12 import NoSLSearch


class FixturePolicy(torch.nn.Module):
    def __init__(self, prefix=()):
        super().__init__()
        self.prefix = iter(prefix)

    def forward(self, inputs):
        kinds = inputs["candidate_kinds"]
        logits = torch.where(kinds == 2, 100., torch.where(kinds == 6, 90., 0.))
        choice = next(self.prefix, None)
        if choice is not None:
            assert inputs["candidate_mask"][0, choice]
            logits[0, choice] = 1000.
        return logits.masked_fill(~inputs["candidate_mask"], -torch.inf), logits.new_zeros(len(kinds))


def test_deck_eval_cpu_collect_fit_and_choose(tmp_path):
    pytest.importorskip("spire")
    native = pytest.importorskip("spire_finediff")
    torch.set_num_threads(1)
    device = torch.device("cpu")
    trace = Path(__file__).parent / "traces" / "Transmogrifier-curse-preview-seed2000000764.jsonl.gz"
    prefix, before_reward = [], None
    with gzip.open(trace, "rt", encoding="utf-8") as stream:
        seed = json.loads(next(stream))["seed"]
        for line in stream:
            row = json.loads(line)
            state = row["snapshot"]
            if before_reward is None and state["screen"]["type"] == "CARD_REWARD":
                before_reward = prefix.copy()
            if (state["run"]["floorNum"] == 15 and state["screen"]["type"] == "MAP"
                    and row["action"]["type"] == "choose"):
                break
            prefix.append(row["legal"].index(row["action"]))
    assert before_reward is not None

    search = NoSLSearch(1, device, 130, storage_slots=2, greedy=True)
    runner = RootCollector(search, torch.Generator().manual_seed(130))
    runner.reset([0], [seed])
    search.batch.reset_to(0, seed, prefix)
    search.provenance[0] = (seed, prefix.copy())
    search.reset_seen([0])
    node = "before_act1_boss"
    roots = runner.capture_roots(FixturePolicy(), {node: 1}, [])
    root, = roots[node]
    saved = search.batch.state_bytes(root["slot"])
    data = tmp_path / "labels"
    manifest = write_collection(iter_label_rows(runner, roots), data, native.deck_vocab_sizes(), variants=1, draws=2)
    assert manifest["roots"] == 1 and manifest["rows"] == 2 and manifest["complete_rows"] == 2
    assert search.batch.state_bytes(root["slot"]) == saved
    with gzip.open(data / manifest["shard"], "rt", encoding="utf-8") as stream:
        raw, variant = [json.loads(line) for line in stream]
    assert raw["draw_seeds"] == variant["draw_seeds"]
    assert len(set(raw["draw_seeds"])) == 2
    assert abs(len(raw["features"]["cards"]) - len(variant["features"]["cards"])) in (1, 2)
    assert all(row["features"]["boss"] == raw["features"]["boss"] for row in (raw, variant))
    assert all(0 <= row["remaining_hp_ratio"] <= 1 and 0 <= row["win_rate"] <= 1 for row in (raw, variant))
    assert set(backend(device).silent_reward_card_ids())

    output = tmp_path / "fit"
    stats = fit(data, output, device, 2, 2, 3e-4, 130)
    assert stats["steps"] == 2 and stats["training_rows"] == 2
    checkpoint = torch.load(output / "deck_eval.pt", weights_only=True)
    evaluator = DeckEvaluator(checkpoint["vocab_sizes"]).eval()
    evaluator.load_state_dict(checkpoint["model"])
    result = choose(FixturePolicy(before_reward), evaluator, device, range(seed, seed + 1), 1)
    assert len(result["episodes"]) == 1
    assert result["search"]["card"]["preview_decisions"] >= 1
    assert result["search"]["card"]["preview_steps"] >= 2
    assert result["search"]["combat"]["preview_steps"] == 0 and result["search"]["map"]["preview_steps"] == 0
    print(json.dumps(dict(pilot=manifest, fit=stats, card=result["search"]["card"])))


def iter_label_rows(runner, roots):
    yield from label_roots(runner, FixturePolicy(), roots, 1, 2, random.Random(130))
