from copy import deepcopy
import gzip
import json
from pathlib import Path

import pytest
import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.e4 import RoomSearch, AllocationEnded, COMBAT_PHASE, PHASE
from train.e4_stage12 import (CLSPolicy, NoSLSearch, NoSLCollector, new_cursor,
    advance_minibatch, save_rank, restore_rank)
from train.model import network_inputs, packed_inputs, distribution


def observation():
    search = RoomSearch(2, 1, torch.device('cpu'), torch.Generator().manual_seed(42), scratch_slots=0)
    collector = NoSLCollector(search, torch.Generator().manual_seed(31))
    collector.fresh([0, 1])
    return network_inputs(search.arrays, search.device, torch.tensor([0, 1]))


def test_new_observation_forward_backward():
    torch.set_num_threads(1)
    model = CLSPolicy()
    inputs = observation()
    logits, value = model(inputs)
    policy, _ = distribution(logits, inputs['candidate_mask'])
    (policy.entropy().mean() + value.mean()).backward()
    assert torch.isfinite(model.cls.grad).all() and model.cls.grad.abs().sum() > 0
    assert all(p.grad is not None and torch.isfinite(p.grad).all() for p in model.parameters())
    print('Stage12 model parameters:', sum(p.numel() for p in model.parameters()))


def test_cls_entity_permutation():
    torch.set_num_threads(1)
    model = CLSPolicy().eval()
    inputs = observation()
    permuted = {k:v.clone() for k,v in inputs.items()}
    order = torch.randperm(inputs['entity_mask'].shape[-1])
    for name in ('tags', 'ids', 'values', 'entity_mask'):
        permuted[name] = inputs[name][..., order]
    inverse = torch.argsort(order)
    refs = inputs['candidate_refs']
    permuted['candidate_refs'] = torch.where(refs > 0, inverse[(refs - 1).clamp_min(0)] + 1, refs)
    with torch.no_grad():
        _, a = model.encode(inputs)
        _, b = model.encode(permuted)
        torch.testing.assert_close(a, b, atol=2e-6, rtol=2e-5)
        for x,y in zip(model(inputs), model(permuted)):
            torch.testing.assert_close(x, y, atol=2e-6, rtol=2e-5)


def test_checkpoint_resume_partial_collection_and_optimizer(tmp_path):
    torch.set_num_threads(1)
    dist.init_process_group('gloo', init_method=(tmp_path / 'gloo').as_uri(), rank=0, world_size=1)
    try:
        torch.manual_seed(71)
        policy = CLSPolicy()
        model = DistributedDataParallel(policy)
        optimizer = torch.optim.Adam(model.parameters(), lr=2e-4, eps=1e-5)
        rng = torch.Generator().manual_seed(51)
        search = NoSLSearch(2, torch.device('cpu'), 67)
        collector = NoSLCollector(search, torch.Generator().manual_seed(31))
        policy.eval()
        fragment = collector.collect(policy, length=2)
        assert not fragment['imitation'].any()
        inputs = packed_inputs(fragment['data'], fragment['offsets'], torch.arange(2), torch.device('cpu'))
        inputs['candidate_mask'] &= fragment['policy_mask'][:2, :inputs['candidate_mask'].shape[1]]
        logits, _ = policy(inputs)
        categorical, _ = distribution(logits, inputs['candidate_mask'])
        torch.testing.assert_close(categorical.log_prob(fragment['actions'][:2]), fragment['logs'][:2])
        cursor = new_cursor(fragment, rng, 1)
        advance_minibatch(model, optimizer, torch.amp.GradScaler('cuda', enabled=False), cursor, rng, 0, 1)
        policy.eval()
        calls = 0
        clock = search.clock
        def interrupt():
            nonlocal calls
            calls += 1
            if calls == 2:
                raise AllocationEnded()
            clock()
        search.clock = interrupt
        with pytest.raises(AllocationEnded):
            collector.collect(policy, length=2)
        search.clock = clock
        path = tmp_path / 'rank.pt'
        save_rank(path, collector, cursor, (1, 1), rng, dict(ready=None, seconds=1.5, decisions_start=0))
        search.evidence_directory = tmp_path
        search.last_heartbeat -= 61
        (tmp_path / 'stop-requested').touch()
        with pytest.raises(AllocationEnded):
            search.clock()
        (tmp_path / 'stop-requested').unlink()
        common = deepcopy(dict(model=policy.state_dict(), optimizer=optimizer.state_dict()))
        expected = collector.collect(policy, length=2)
        expected_state = [search.batch.state_bytes(s) for s in range(2)]
        restored_policy = CLSPolicy().eval()
        restored_policy.load_state_dict(common['model'])
        restored_optimizer = torch.optim.Adam(restored_policy.parameters(), lr=2e-4, eps=1e-5)
        restored_optimizer.load_state_dict(common['optimizer'])
        resumed_search = NoSLSearch(2, torch.device('cpu'), 0)
        resumed = NoSLCollector(resumed_search, torch.Generator().manual_seed(0))
        resumed_rng = torch.Generator()
        restored_cursor, identity, round_state = restore_rank(torch.load(path, weights_only=False), resumed, torch.device('cpu'), resumed_rng)
        assert identity == (1, 1) and round_state['seconds'] == 1.5
        assert restored_cursor['epoch'] == cursor['epoch']
        for key in optimizer.state_dict()['state']:
            for name, value in optimizer.state_dict()['state'][key].items():
                torch.testing.assert_close(value, restored_optimizer.state_dict()['state'][key][name], rtol=0, atol=0)
        actual = resumed.collect(restored_policy, length=2)
        for key in expected:
            torch.testing.assert_close(actual[key], expected[key], rtol=0, atol=0)
        assert expected_state == [resumed_search.batch.state_bytes(s) for s in range(2)]
        assert torch.equal(search.rng.get_state(), resumed_search.rng.get_state())
    finally:
        dist.destroy_process_group()


def test_fast_masks_equal_original_per_slot():
    torch.set_num_threads(1)
    old = RoomSearch(2, 1, torch.device('cpu'), torch.Generator().manual_seed(81), scratch_slots=0)
    fast = NoSLSearch(2, torch.device('cpu'), 81)
    with gzip.open(Path(__file__).resolve().parents[1] / 'inventory/e4-c88-stage6-combat-exam.json.gz', 'rt') as stream:
        scene = json.load(stream)['groups']['act1_boss_guardian'][0]
    slots = [0, 1]
    for search in (old, fast):
        collector = NoSLCollector(search, torch.Generator().manual_seed(31))
        collector.reset(slots, [scene['seed'], 2000000001])
        search.batch.reset_to_many([0], [scene['seed']], [scene['prefix']])
        search.provenance[0] = (scene['seed'], scene['prefix'].copy())
        search.reset_seen(slots)
    progress = old.progress_for(slots)
    assert progress[0][PHASE] == COMBAT_PHASE and progress[1][PHASE] != COMBAT_PHASE
    rng = torch.Generator().manual_seed(53)
    for _ in range(12):
        a = old.policy_masks(slots)
        b = fast.policy_masks(slots)
        assert torch.equal(a, b)
        choices = torch.multinomial(a.float(), 1, generator=rng).flatten().tolist()
        old.step(slots, choices)
        fast.step(slots, choices)
    assert [old.batch.state_bytes(s) for s in slots] == [fast.batch.state_bytes(s) for s in slots]
