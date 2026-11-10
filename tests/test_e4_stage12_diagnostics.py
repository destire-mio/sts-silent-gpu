from types import SimpleNamespace

import pytest
import torch

from tools.e4_stage12 import startup_value_setting
from train import e4_evaluation_ledger as ledger, e4_stage12 as stage
from train.e4 import ACT, BOSS_ROOM, COMBAT_PHASE, FLOOR, HP, MAP, PHASE, ROOM, SCREEN
from train.e4_midstart import midstart_policy


def test_stage12_diagnostics_report_effective_nonforced_and_per_act_data(monkeypatch):
    assert midstart_policy([31, 32, 256, 256])["fresh"] == 1
    policy = midstart_policy([32, 32, 31, 32])
    assert [policy[k] for k in ("fresh", "act1_boss", "act2", "act2_boss", "act3")] == [.45, .10, .35, 0., .10]
    assert startup_value_setting({"standalone_value_coefficient": .7}, False) == {
        "coefficient": .7, "source": "fixed"}
    assert startup_value_setting({"value_calibration": dict(target=1., completed_updates=10)}, False) == {
        "coefficient": .75, "source": "ramp"}

    legal = torch.tensor([[True, True], [True, True], [True, False], [True, True]])
    fragment = dict(actions=torch.zeros(4, dtype=torch.long), action_valid=torch.tensor([True, True, True, False]),
                    decision_types=torch.tensor([0, 0, 1, 2]), advantages=torch.tensor([-2., 4., 99., 99.]),
                    policy_mask=legal, data=None, offsets=None)
    after = torch.tensor([[0., 2.], [2., 0.], [0., -torch.inf], [2., 0.]])
    before = torch.tensor([[2., 0.], [2., 0.], [0., -torch.inf], [2., 0.]]).log_softmax(-1)
    monkeypatch.setattr(stage, "packed_inputs", lambda _data, _offsets, ids, _device: {
        "candidate_mask": legal[ids], "ids": ids})
    compare = torch.arange(3)
    drift = stage.measure_decision_drift(lambda x: (after[x["ids"]], None), fragment, [(compare, before[:3])])
    report = stage.decision_training_summary(dict(fragment=fragment, decision_diagnostics_drift=drift,
        decision_diagnostics_actor=torch.tensor([[1., 3., 99., 99.], [2., 4., 99., 99.]])))
    combat = report["combat"]
    assert combat["samples"] == 2 and combat["advantage_mean"] == 1. and combat["advantage_std"] == 3.
    assert combat["policy_loss_samples"] == 4 and combat["policy_loss"] == 2.5
    assert combat["final_minibatch_samples"] == 2 and combat["argmax_flip_rate"] == .5
    assert combat["post_pre_kl"] == pytest.approx(.761594, rel=1e-5)
    assert report["card_reward"]["samples"] == 0 and report["card_reward"]["post_pre_kl"] is None

    arrays = dict(tags=torch.zeros((1, 4, 5), dtype=torch.uint8), ids=torch.zeros((1, 4, 5), dtype=torch.int16))
    arrays["tags"][0, 0, 3], arrays["ids"][0, 0, 3] = 11, 9
    boss_id = ledger.ENCOUNTERS.index("Champ")
    arrays["ids"][0, 3, 3] = boss_id
    search = SimpleNamespace(lanes=1, size=1, device=torch.device("cpu"), arrays=arrays,
                             failed={}, last_step_progress={},
                             batch=SimpleNamespace(combat_turns_tensors=lambda _turns: None))
    collector = object.__new__(ledger.EvaluationLedgerCollector)
    collector.search, collector.records, collector.extra_details = search, [None], True
    collector.completed, collector.turns = [], torch.zeros(1, dtype=torch.long)
    monkeypatch.setattr(stage.NoSLCollector, "reset", lambda *_args: None)
    monkeypatch.setattr(stage.NoSLCollector, "choose", lambda *_args: ([0], [.5], [0.], [False]))
    monkeypatch.setattr(ledger, "visible_state", lambda *_args: {"encounter": ["Champ"]})
    collector.reset([0], [7])
    p = [0] * 9
    p[ACT], p[SCREEN], p[FLOOR] = 2, MAP, 18
    collector.choose(None, [0], {0: p})
    p[ROOM], p[PHASE], p[HP], p[FLOOR] = BOSS_ROOM, COMBAT_PHASE, 41, 33
    collector.choose(None, [0], {0: p})
    frame = dict(actions=[(3, None)], screen=10, rest_options=[1], deck_size=1)
    search.frames = lambda *_args: {0: frame}
    monkeypatch.setattr(stage.NoSLCollector, "advance", lambda *_args: ([0.], [False]))
    p[ACT], p[PHASE], p[SCREEN] = 3, 0, 10
    collector.advance([0], [0], {0: p})
    frame["rest_options"], frame["deck_size"] = [2], 0
    collector.advance([0], [0], {0: p})
    record = collector.records[0]
    assert record["act2_boss_id"] == boss_id
    assert record["boss_entries"] == [dict(act=2, floor=33, hp=41, encounter=["Champ"])]
    exam = ledger.exam_breakdown([record])
    assert exam["act2_boss_counts"] == {"Champ": 1}
    assert exam["by_act"]["2"]["boss_entry_hp"] == {"samples": 1, "mean": 41}
    assert [exam["by_act"]["3"][k] for k in ("rests", "upgrades", "card_removals")] == [1, 1, 1]
