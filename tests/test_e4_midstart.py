from copy import deepcopy
from collections import Counter
import torch
from train.e4 import ACT, FLOOR, PHASE, ROOM, COMBAT_PHASE, BOSS_ROOM, MAX_REWARD
from train.e4_stage12 import CLSPolicy, NoSLSearch, NoSLCollector, collector_snapshot, restore_collector
from train.e4_midstart import MidstartCollector

def make():
    torch.set_num_threads(1)
    search = NoSLSearch(2, torch.device('cpu'), 101, storage_slots=512)
    collector = MidstartCollector(search, torch.Generator().manual_seed(33))
    collector.fresh([0, 1])
    return collector

def combat(collector, model):
    while True:
        progress = collector.search.progress_for([0])
        if progress[0][PHASE] == COMBAT_PHASE:
            return
        actions, _, _, _ = collector.choose(model, [0], progress)
        collector.advance([0], actions, progress)

def test_pool_clone_and_randomized_checkpoint_resume():
    torch.manual_seed(8)
    model = CLSPolicy().eval()
    c = make()
    combat(c, model)
    original = c.search.batch.state_bytes(0)
    c.retain(0, 0)
    assert c.search.batch.state_bytes(c.search.lanes) == original
    c.restart(0, 1)
    assert c.search.batch.state_bytes(1) == original
    assert c.starts[1] == 'act1_boss'
    p = c.search.progress_for([1])
    actions, _, _, _ = c.choose(model, [1], p)
    assert len(c.randomizations[1]) == 1
    assert c.search.batch.state_bytes(1) != original
    c.advance([1], actions, p)
    saved = collector_snapshot(c)
    expected = [c.search.batch.state_bytes(s) for s in [0, 1, c.search.lanes]]
    resumed = make()
    restore_collector(resumed, deepcopy(saved))
    assert expected == [resumed.search.batch.state_bytes(s) for s in [0, 1, resumed.search.lanes]]
    assert torch.equal(c.seed_rng.get_state(), resumed.seed_rng.get_state())
    assert torch.equal(c.search.rng.get_state(), resumed.search.rng.get_state())
    for collector in [c, resumed]:
        p = collector.search.progress_for([1])
        choices, _, _, _ = collector.choose(model, [1], p)
        collector.advance([1], choices, p)
    assert c.search.batch.state_bytes(1) == resumed.search.batch.state_bytes(1)
    assert c.rewards[1] == resumed.rewards[1]

def test_pool_reward_prefix_and_native_state_stay_frozen():
    c = make()
    c.rewards[0].floors = set(range(1, 18))
    c.rewards[0].bosses = {(1, 16)}
    c.rewards[0].reach['act1_boss'] = True
    c.rewards[0].total = 22
    c.retain(1, 0)
    stored = c.search.lanes + 256
    native = c.search.batch.state_bytes(stored)
    c.restart(1, 1)
    before = [17, 2, 0, 0, 9, 0, 0, 0, 50]
    after = before.copy(); after[FLOOR] = 18
    assert c.rewards[1].advance(before, after) == 1 / MAX_REWARD
    assert c.rewards[1].total == 23
    assert c.pools[1][0]['reward'].total == 22
    assert c.search.batch.state_bytes(stored) == native

def test_warmup_and_restart_mix(monkeypatch):
    c = make()
    calls = Counter()
    monkeypatch.setattr(NoSLCollector, 'fresh', lambda self, slots: calls.update({'fresh':len(slots)}))
    monkeypatch.setattr(c, 'restart', lambda pool, slot: calls.update({pool:1}))
    c.pools = [[{}] * 32, [{}] * 31]
    c.fresh(list(range(10000)))
    assert calls == {'fresh':10000}
    calls.clear()
    c.pools = [[{}] * 32, [{}] * 32]
    c.fresh(list(range(10000)))
    assert sum(calls.values()) == 10000
    assert 4800 < calls['fresh'] < 5200
    assert 3800 < calls[1] < 4200
    assert 800 < calls[0] < 1200

def test_pool_capacity_and_old_checkpoint_migration():
    c = make()
    old = NoSLCollector(NoSLSearch(2, torch.device('cpu'), 1), torch.Generator().manual_seed(5))
    old.fresh([0, 1])
    restore_collector(c, collector_snapshot(old))
    assert c.pools == [[], []]
    assert [c.search.batch.state_bytes(s) for s in [0, 1]] == [old.search.batch.state_bytes(s) for s in [0, 1]]
    for _ in range(260):
        c.retain(0, 0)
    assert len(c.pools[0]) == 256 and c.pools[1] == []
    assert c.search.probe_slots[0] == c.search.lanes + 512


def test_faulted_episode_never_enters_pool(monkeypatch):
    c = make()
    c.search.failed[0] = 'engine_fault:1'
    captured = []
    monkeypatch.setattr(c, 'retain', lambda pool, slot: captured.append((pool, slot)))
    monkeypatch.setattr(NoSLCollector, 'choose', lambda self, model, slots, progress: ([0], [0.], [0.], [False]))
    c.choose(None, [0], {0:[17, 2, 1, 0, 0, 0, 0, 0, 1]})
    assert captured == []
