"""#140-only regression: a measured higher peak must not ratchet the anchor."""
from copy import deepcopy
from types import SimpleNamespace
import torch
from train import e4_expert_iteration as ex


def test_finish_never_ratchets_pre_expert_peak(monkeypatch):
    monkeypatch.setattr(ex.dist,'all_reduce',lambda *a,**k:None)
    measured = {'peak':100000}
    monkeypatch.setattr(ex,'gpu_memory',lambda device:dict(measured))
    obj = object.__new__(ex.ExpertIteration)
    obj.device = torch.device('cpu')
    obj.setup_seconds = 0.
    obj.collector = SimpleNamespace(completed=[])
    obj.memory = dict(previous_peak=1000,native_per_lane=10,working_per_lane=20,last_limit=1250)
    obj.lanes = obj.capacity = 1
    obj.throughput = None
    for peak in (100000,200000,400000):
        measured['peak'] = peak
        obj.round = dict(records=[],agreement=0,seconds=1.,lanes=1,ticks=64)
        row = obj.finish({'expert_ce_seconds':0.,'expert_ce_stats':[0.,0.]},100.,None)
        assert obj.memory['previous_peak'] == 1000
        assert row['expert_memory_anchor_peak_bytes'] == 1000
        assert row['expert_gpu_peak_bytes'] == peak
        assert row['expert_memory_growth_limit_bytes'] == 1250


def test_native_checkpoint_restores_fixed_anchor_and_later_measurements_do_not_replace_it(tmp_path,monkeypatch):
    from train.e4_stage12 import NoSLSearch
    from train.e4_midstart import MidstartCollector
    source = MidstartCollector(NoSLSearch(1,torch.device('cpu'),21,probe_slots=128),
                              torch.Generator().manual_seed(22),rank=0)
    obj = ex.ExpertIteration(source,tmp_path,float('inf'),0)
    obj.memory = dict(previous_peak=123456,native_per_lane=100,working_per_lane=200,last_limit=154320)
    saved = obj.state()
    expected = deepcopy(saved['memory'])
    restored = ex.ExpertIteration(source,tmp_path,float('inf'),0,saved)
    assert restored.memory == expected
    assert restored.state()['memory']['previous_peak'] == 123456
    # Higher current usage cannot change the fixed growth ceiling.
    _,limit = ex.memory_limited_lanes(512,1,1,999999,restored.memory['previous_peak'],
                                     10**12,100,200)
    assert limit == 154320
