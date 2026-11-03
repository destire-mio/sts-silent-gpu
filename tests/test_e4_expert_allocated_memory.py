"""#144: allocated-byte projection and rank-coordinated optional-expert OOMs."""
from copy import deepcopy
from types import SimpleNamespace
from unittest.mock import patch

import pytest
import torch
import torch.distributed as dist
import torch.multiprocessing as mp
from torch.nn.parallel import DistributedDataParallel as DDP

from train import e4_expert_iteration as ex

GiB = 1024**3


def shell(lanes=4):
    obj = object.__new__(ex.ExpertIteration)
    obj.device = torch.device('cpu')
    obj.rank = 0
    obj.lanes = obj.capacity = lanes
    obj.memory = {}
    obj.throughput = dict(lanes=2, samples_per_second=1.)
    obj.round = dict(update=1, lanes=lanes, records=[], agreement=0,
                     preview_steps=0, seconds=0., ticks=2, done=False)
    obj.search = SimpleNamespace(labels={1:1},frame_cache={1:1},mask_cache={1:1})
    obj.collector = SimpleNamespace(completed=[])
    obj.collect = lambda *args:None
    obj.fragment = lambda:dict(actions=torch.empty(0,dtype=torch.int64))
    return obj


def test_gate_uses_allocated_ppo_bytes_and_fixed_physical_headroom(monkeypatch):
    # A full reserved cache / obsolete full-card anchor cannot block lane3.
    for reserved in (5*GiB,16*GiB):
        monkeypatch.setattr(torch.cuda,'mem_get_info',lambda d:(0,16*GiB))
        monkeypatch.setattr(torch.cuda,'memory_allocated',lambda d:4*GiB)
        monkeypatch.setattr(torch.cuda,'max_memory_allocated',lambda d:5*GiB)
        monkeypatch.setattr(torch.cuda,'memory_reserved',lambda d:reserved)
        measured = ex.gpu_memory(torch.device('cuda'))
        assert measured['peak'] == 5*GiB and 'used' not in measured
        lanes,limit = ex.memory_limited_lanes(512,measured['peak'],measured['total'],GiB)
        assert limit == 14.5*GiB and lanes == 9
    assert ex.memory_limited_lanes(1,16*GiB,16*GiB,GiB)[0] == 1


def test_gate_keeps_all_rank_minimum_and_recent_measured_lane_cost(monkeypatch):
    obj = shell(8)
    obj.memory = dict(ppo_allocated_peak=5*GiB,previous_ppo_allocated_peak=4*GiB,
                      native_per_lane=GiB//4,working_per_lane=GiB//4,
                      measured_per_lane=GiB,previous_peak=16*GiB)
    monkeypatch.setattr(ex,'gpu_memory',lambda d:dict(total=16*GiB))
    monkeypatch.setattr(ex.dist,'is_initialized',lambda:True)
    def reduce_min(flag,op):
        assert op == dist.ReduceOp.MIN
        flag.fill_(3)
    monkeypatch.setattr(ex.dist,'all_reduce',reduce_min)
    obj.prepare_lanes()
    assert obj.lanes == 3 and obj.memory['lane_cost'] == GiB
    assert obj.memory['projection_base'] == 5*GiB
    assert obj.memory['previous_peak'] == 16*GiB # Historical metadata only.
    obj.memory['measured_per_lane'] = GiB//8
    obj.prepare_lanes()
    assert obj.memory['lane_cost'] == GiB//2 # Recent measurement, not lifetime max.


def test_collection_peak_increment_is_per_lane_and_reset_before_ppo(monkeypatch):
    obj = shell()
    readings = iter([dict(allocated_peak=2000),dict(allocated=1000),
                     dict(allocated_peak=1800)])
    monkeypatch.setattr(ex,'gpu_memory',lambda d:next(readings))
    resets=[]
    monkeypatch.setattr(torch.cuda,'reset_peak_memory_stats',lambda d:resets.append(d))
    monkeypatch.setattr(ex.dist,'all_reduce',lambda *a,**k:None)
    obj.collect_and_fragment(None,1,10.)
    assert obj.memory['measured_per_lane'] == 200
    assert obj.memory['collection_allocated_peak'] == 1800
    assert obj.memory['ppo_allocated_peak'] == 2000
    assert len(resets) == 2


def test_oom_discards_optional_labels_halves_and_releases_cache(monkeypatch,capsys):
    obj = shell()
    obj.device = torch.device('cuda')
    obj.round['records']=[dict(data=torch.ones(1))]
    cleared=[]
    monkeypatch.setattr(torch.cuda,'empty_cache',lambda:cleared.append(True))
    obj.discard_oom('CE')
    assert obj.lanes == 2 and obj.round['records'] == []
    assert obj.round['oom_stage'] == 'CE' and obj.throughput is None
    assert cleared == [True] and 'PPO_continues' in capsys.readouterr().out


def test_failed_native_growth_restores_previously_fitting_geometry(monkeypatch):
    obj=shell(2)
    obj.lanes=4
    obj.source_collector=object()
    obj.setup_seconds=0.
    obj.state=lambda:dict(collector={'old':True})
    monkeypatch.setattr(ex,'gpu_memory',lambda d:None)
    monkeypatch.setattr(ex.dist,'is_initialized',lambda:False)
    monkeypatch.setattr(torch.cuda,'empty_cache',lambda:None)
    calls=[]
    def restore(capacity,saved,source,*,growing):
        calls.append((capacity,growing))
        obj.search=SimpleNamespace(labels={},frame_cache={},mask_cache={})
        obj.collector=SimpleNamespace(completed=[])
        if growing:raise torch.cuda.OutOfMemoryError('native growth OOM')
    obj._restore_capacity=restore
    with pytest.raises(torch.cuda.OutOfMemoryError):obj.prepare_lanes()
    assert calls==[(4,True),(2,False)] and obj.capacity==2
    assert obj.collector is not None and obj.search is not None


@pytest.mark.parametrize('stage',['collection','fragment'])
def test_collection_and_fragment_oom_both_return_empty_labels(monkeypatch,stage):
    obj = shell()
    monkeypatch.setattr(ex.dist,'all_reduce',lambda *a,**k:None)
    def oom(*args):raise torch.cuda.OutOfMemoryError('injected optional expert OOM')
    if stage == 'collection':obj.collect=oom
    else:
        original=obj.fragment
        calls=[0]
        def fragment():
            calls[0]+=1
            if calls[0]==1:oom()
            return original()
        obj.fragment=fragment
    result=obj.collect_and_fragment(None,1,10.)
    assert len(result['actions'])==0 and obj.lanes==2
    assert obj.round['done'] and obj.round['oom_stage']=='collection'


def ddp_recovery_worker(rank,rendezvous):
    torch.set_num_threads(1)
    dist.init_process_group('gloo',init_method=rendezvous,rank=rank,world_size=2)
    try:
        obj=shell()
        obj.rank=rank
        def collect(*args):
            if rank==1:raise torch.cuda.OutOfMemoryError('one rank collection OOM')
        obj.collect=collect
        obj.collect_and_fragment(None,1,10.)
        assert obj.lanes==2 and obj.round['records']==[]
        torch.manual_seed(144)
        model=DDP(torch.nn.Linear(3,1),broadcast_buffers=False)
        reference=DDP(deepcopy(model.module),broadcast_buffers=False)
        optimizer=torch.optim.Adam(model.parameters(),lr=.01)
        expected_optimizer=torch.optim.Adam(reference.parameters(),lr=.01)
        data=torch.tensor([[rank+1.,2.,3.]])
        def ce(model,scaler,cursor,*args):
            with model.no_sync():
                model(data).sum().backward()
            cursor['expert_ce_stats'][0]+=123.
            torch.rand(3)
            if rank==1:raise torch.cuda.OutOfMemoryError('one rank CE OOM after partial gradient')
        for _ in range(2):
            optimizer.zero_grad(set_to_none=True)
            expected_optimizer.zero_grad(set_to_none=True)
            cursor=dict(order=torch.arange(1),expert_total=2,expert={'labels':1},
                        expert_ce_stats=[3.,2.],finite=torch.tensor(True))
            before=torch.get_rng_state()
            with patch.object(ex,'_expert_backward',ce):
                ex.expert_backward(model,None,cursor,rank,2,1,obj.discard_oom)
            assert torch.equal(torch.get_rng_state(),before)
            assert cursor['expert_total']==0 and cursor['expert'] is None
            assert cursor['expert_ce_stats']==[3.,2.] and bool(cursor['finite'])
            model(data).square().mean().backward()
            reference(data).square().mean().backward()
            optimizer.step();expected_optimizer.step()
            for a,b in zip(model.parameters(),reference.parameters()):
                torch.testing.assert_close(a,b,rtol=0,atol=0)
            assert all(int(s['step'])==_+1 for s in optimizer.state.values())
    finally:dist.destroy_process_group()


def test_two_rank_collection_and_partial_ce_oom_preserve_ppo_adam(tmp_path):
    mp.spawn(ddp_recovery_worker,args=((tmp_path/'gloo144').as_uri(),),nprocs=2,join=True)
