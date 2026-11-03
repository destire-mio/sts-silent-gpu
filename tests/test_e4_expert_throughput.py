from types import SimpleNamespace

import pytest
import torch

from train import e4_expert_iteration as expert


@pytest.mark.parametrize('lanes,extra,ticks,speed,previous,wanted', [
    (1, 21., 12, 10., None, 2),
    (2, 21., 12, 12., dict(lanes=1, samples_per_second=10.), 4),
    (2, 21., 12, 11., dict(lanes=1, samples_per_second=10.), 2),
    (4, 21., 12, 9., dict(lanes=2, samples_per_second=10.), 2),
    (4, 21., 64, 9., dict(lanes=2, samples_per_second=10.), 8),
    (8, 31., 64, 40., dict(lanes=4, samples_per_second=10.), 4),
    (1, 31., 10, 10., None, 1),
    (512, 20., 64, 10., None, 512),
])
def test_deadline_throughput_growth_and_overhead_precedence(lanes, extra, ticks, speed, previous, wanted):
    assert expert.next_lane_count(lanes, extra, 100., ticks=ticks,
        samples_per_second=speed, previous=previous) == wanted


def test_finish_uses_global_samples_max_seconds_and_min_ticks(monkeypatch):
    obj = object.__new__(expert.ExpertIteration)
    obj.device = torch.device('cpu')
    obj.setup_seconds = 0.
    obj.collector = SimpleNamespace(completed=[])
    obj.round = dict(records=[{}] * 10, agreement=7, seconds=1., lanes=2, ticks=64)
    obj.lanes = obj.capacity = 2
    obj.memory = dict(previous_peak=1000, last_limit=1250)
    obj.throughput = dict(lanes=1, samples_per_second=10.)

    def collective(tensor, op=None):
        if len(tensor) == 6:
            tensor[0] = 120
        else:
            tensor[:] = torch.tensor([21., 10., 1., -12.])
    monkeypatch.setattr(expert.dist, 'all_reduce', collective)
    row = obj.finish(dict(expert_ce_seconds=1., expert_ce_stats=[1., 1.]), 121., None)
    assert row['expert_samples_per_second'] == 12.
    assert row['expert_ticks'] == 12
    assert row['expert_next_lanes'] == 4
    assert obj.throughput == dict(lanes=2, samples_per_second=12.)
    assert obj.memory['previous_peak'] == 1000
