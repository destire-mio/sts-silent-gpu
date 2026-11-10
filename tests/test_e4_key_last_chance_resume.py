import torch

from train.e4 import ACT, KEYS, MAP, SCREEN
from train.e4_key_last_chance import EMERALD, REST, SAPPHIRE
from train.e4_stage12 import NoSLCollector, NoSLSearch, collector_snapshot, restore_collector


def test_resume_defers_existing_act3_only_until_the_next_episode(monkeypatch):
    search = NoSLSearch(2, torch.device("cpu"), 199, storage_slots=1, probe_slots=1, key_last_chance=True)
    collector = NoSLCollector(search, torch.Generator().manual_seed(199))
    collector.reset([0, 1], [101, 102])
    monkeypatch.setattr(search, "progress_for", lambda slots: {0: {ACT: 3}, 1: {ACT: 2}})
    search.defer_key_last_chance_on_resume([0, 1])
    assert search.key_last_chance == {0: None}

    progress = [0] * 9
    progress[ACT], progress[SCREEN], progress[KEYS] = 3, MAP, EMERALD | SAPPHIRE
    search.mainline_progress = {0: progress, 1: progress}
    frame = {"key_map_nodes": [(0, 14, REST, 0b1000, False), (1, 14, 0, 0b1000, False)],
             "key_map_actions": {0: (0, 14), 1: (1, 14)}}
    search.frame_cache.update({0: frame, 1: frame})
    mask = torch.ones((2, 2), dtype=torch.bool)
    search.mask_last_chance_keys([0, 1], [0, 1], mask)
    assert mask.tolist() == [[True, True], [True, False]]

    state = collector_snapshot(collector)
    search.key_last_chance.clear()
    restore_collector(collector, state)
    assert search.key_last_chance[0] is None

    # Capturing a pool state does not defer a new episode started from that pool.
    search.clone([0], [2])
    assert 2 not in search.key_last_chance
    search.clone([2], [0])
    assert 0 not in search.key_last_chance
    search.defer_key_last_chance_on_resume([0, 1])
    collector.reset([0], [103])
    assert 0 not in search.key_last_chance
