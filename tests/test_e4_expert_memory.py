"""#139-only CPU regressions: lane ceiling, memory projection, dynamic storage, replay."""
from copy import deepcopy
import torch
from train import e4_expert_iteration as ex
from train.e4_stage12 import NoSLSearch, CLSPolicy
from train.e4_midstart import MidstartCollector
from train.model import ModelConfig


def test_expert_ceiling_is_512_and_calibration_still_starts_at_one(tmp_path):
    source = MidstartCollector(NoSLSearch(1,torch.device('cpu'),11,probe_slots=128),
                              torch.Generator().manual_seed(12),rank=0)
    expert = ex.ExpertIteration(source,tmp_path,float('inf'),0)
    assert ex.MAX_LANES == 512
    assert ex.next_lane_count(256,.01,100,ticks=64,samples_per_second=10.,previous=None) == 512
    assert expert.lanes == expert.capacity == expert.search.lanes == 1
    assert len(expert.search.available) == 32
    assert expert.search.size == 1+32+128


def test_memory_projection_stops_before_allocated_byte_headroom():
    allowed,limit = ex.memory_limited_lanes(512,1000,1536*1024**2+1250,30)
    assert limit == 1250 and allowed == 8
    assert 1000+allowed*30 <= limit
    assert 1000+(allowed+1)*30 > limit
    allowed,_ = ex.memory_limited_lanes(512,1250,1536*1024**2+1250,30)
    assert allowed == 1


def test_lane_estimate_and_physical_headroom_are_counted():
    allowed,_ = ex.memory_limited_lanes(20,1000,1536*1024**2+2000,120)
    assert allowed == 8
    _,limit = ex.memory_limited_lanes(512,1,2*1024**3,1)
    assert limit == 512*1024**2


def test_native_capacity_growth_preserves_live_game_preview_rng_and_ppo(tmp_path):
    source = MidstartCollector(NoSLSearch(1,torch.device('cpu'),22,probe_slots=128),
                              torch.Generator().manual_seed(23),rank=0)
    source_before = deepcopy(source.state())
    policy = CLSPolicy(ModelConfig(32,1,1,64)).eval()
    obj = ex.ExpertIteration(source,tmp_path,float('inf'),0)
    obj.round = dict(update=1,ticks=63,records=[],seconds=0.,agreement=0,
                     preview_steps=0,done=False,lanes=1)
    obj.collect(policy,1,100.)
    original = obj.state()
    # Growth happens between completed rounds; pending training labels stay in
    # the original learner cursor until its existing Adam steps complete.
    obj.round = None
    obj.lanes = 4
    obj.prepare_lanes()
    assert obj.capacity == obj.search.lanes == 4
    assert len(obj.search.available) == 4*32
    assert obj.search.size == 4+4*32+128
    assert obj.collector.seeds[0] == original['collector']['seeds'][0]
    assert obj.collector.paths[0] == original['collector']['paths'][0]
    assert obj.collector.seeds[1:] == [None]*3
    assert torch.equal(obj.search.lookahead.rng.get_state(),original['preview_rng'])
    assert torch.equal(obj.collector.seed_rng.get_state(),original['collector']['seed_rng'])
    assert torch.equal(obj.search.rng.get_state(),original['collector']['search_rng'])
    assert source.seeds == source_before['seeds'] and source.paths == source_before['paths']
    assert torch.equal(source.seed_rng.get_state(),source_before['seed_rng'])
    saved = obj.state()
    restored = ex.ExpertIteration(source,tmp_path,float('inf'),0,saved)
    assert restored.capacity == 4
    assert restored.collector.seeds == obj.collector.seeds
    assert restored.collector.paths == obj.collector.paths
    assert torch.equal(restored.search.lookahead.rng.get_state(),obj.search.lookahead.rng.get_state())


def test_unfinished_round_keeps_its_geometry_and_existing_labels():
    obj = object.__new__(ex.ExpertIteration)
    obj.round = dict(update=2,done=True,ticks=64,records=[{'action':1}])
    obj.prepare_lanes = lambda:(_ for _ in ()).throw(AssertionError('must not resize a pending round'))
    obj.collect(None,2,100.)
    assert obj.round['records'] == [{'action':1}]
