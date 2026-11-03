from copy import deepcopy
import random

import numpy as np
import torch

from train import OBSERVATION_VERSION
from train.e4_midstart import MidstartCollector
from train.e4_stage12 import (NoSLSearch, collector_snapshot, restart_rank_observation,
                             restore_rank)
from train.e4_expert_iteration import ExpertIteration


def collector(seed):
    return MidstartCollector(NoSLSearch(2, torch.device('cpu'), seed,
        storage_slots=1024, probe_slots=128), torch.Generator().manual_seed(seed + 1), rank=0)


def test_old_live_fragment_discarded_four_pools_replayed_and_rng_retained():
    torch.set_num_threads(1)
    source = collector(141)
    source.fresh([0, 1])
    for pool in range(4):
        source.retain(pool, pool % 2)
    pool_states = [source.search.batch.state_bytes(2 + pool * 256) for pool in range(4)]
    source.decisions = 321
    snapshot = collector_snapshot(source)
    ppo_rng = torch.Generator().manual_seed(73)
    old = dict(collector=snapshot, cursor=dict(optimizer_steps=54),
        round_state=dict(ready={'old_observations': True}, seconds=10., decisions_start=0),
        ppo_rng=ppo_rng.get_state(), torch_rng=torch.get_rng_state(),
        python_rng=random.getstate(), numpy_rng=np.random.get_state(), update=(1414, 150))
    target = collector(99)
    changed = restart_rank_observation(old, target)
    assert old['cursor']['optimizer_steps'] == 54
    assert changed['cursor'] is None and changed['round_state']['ready'] is None
    assert changed['round_state']['decisions_start'] == 321
    restored_rng = torch.Generator()
    cursor, identity, round_state = restore_rank(changed, target, torch.device('cpu'), restored_rng)
    assert cursor is None and identity == (1414, 150)
    assert target.seeds == [None, None] and target.paths == [[], []]
    assert target.pending == source.pending
    assert [len(pool) for pool in target.pools] == [1, 1, 1, 1]
    assert pool_states == [target.search.batch.state_bytes(2 + pool * 256) for pool in range(4)]
    assert torch.equal(target.seed_rng.get_state(), snapshot['seed_rng'])
    assert torch.equal(target.search.rng.get_state(), snapshot['search_rng'])
    assert torch.equal(restored_rng.get_state(), old['ppo_rng'])
    assert torch.equal(torch.get_rng_state(), old['torch_rng'])
    assert random.getstate() == old['python_rng']
    actual = np.random.get_state()
    assert actual[0] == old['numpy_rng'][0]
    np.testing.assert_array_equal(actual[1], old['numpy_rng'][1])
    assert actual[2:] == old['numpy_rng'][2:]


def test_expert_transition_clears_old_labels_preserves_anchor_rng_and_migrates_once(tmp_path):
    source = collector(33)
    old = ExpertIteration(source, tmp_path, float('inf'), 0)
    old.collector.fresh([0])
    old.round = dict(update=1415, ticks=10, records=[dict(data=torch.tensor([1], dtype=torch.uint8),
        mask=torch.ones(128, dtype=torch.bool), action=1)], seconds=2., agreement=1,
        preview_steps=5, done=False, lanes=1)
    old.memory = dict(previous_peak=16800940032, native_per_lane=100, working_per_lane=200)
    saved = old.state()
    saved.pop('observation_version')
    resumed = ExpertIteration(source, tmp_path, float('inf'), 0, saved)
    assert resumed.round is None and resumed.collector.seeds == [None]
    assert resumed.memory == saved['memory']
    assert torch.equal(resumed.search.lookahead.rng.get_state(), saved['preview_rng'])
    assert torch.equal(resumed.collector.seed_rng.get_state(), saved['collector']['seed_rng'])
    assert torch.equal(resumed.search.rng.get_state(), saved['collector']['search_rng'])
    resumed.collector.fresh([0])
    resumed.round = deepcopy(old.round)
    resumed.throughput = dict(lanes=1, samples_per_second=3.)
    current = resumed.state()
    assert current['observation_version'] == OBSERVATION_VERSION == 14
    again = ExpertIteration(source, tmp_path, float('inf'), 0, current)
    assert again.round['ticks'] == 10 and len(again.round['records']) == 1
    assert again.collector.seeds == current['collector']['seeds']
    assert again.throughput == current['throughput']
    assert again.memory['previous_peak'] == 16800940032
