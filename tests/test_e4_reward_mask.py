from copy import deepcopy

from train.e4_rewards import RewardHistory


def test_free_reward_mask_and_opened_card_memory():
    history = RewardHistory()
    frame = dict(screen=7, floor=1, rewards=[(2, 0, 20), (3, 9, 0), (1, 0, 0), (1, 0, 0), (4, 8, 0), (6, 0, 0)],
                 actions=[(3, i) for i in range(6)] + [(6, None)], empty_potion_slot=True,
                 rest_options=[None] * 7, gold=99, potion_count=0, relic_count=1, deck_size=12)
    mask, rule = history.mask(frame, [True] * 7)
    assert mask == [True, False, False, False, False, False, False] and rule == 'gold'
    history.before_step(frame, 0, [True] * 7)
    after = deepcopy(frame)
    after.update(rewards=frame['rewards'][1:], actions=[(3, i) for i in range(5)] + [(6, None)], gold=119)
    history.after_step(frame, after, 0)
    frame = after
    assert history.mask(frame, [True] * 6) == ([True, False, False, False, False, False], 'potion')
    frame['empty_potion_slot'] = False
    assert history.mask(frame, [True] * 6) == ([True, True, True, True, True, False], 'card')
    frame['empty_potion_slot'] = True
    history.before_step(frame, 0, [True] * 6)
    after = deepcopy(frame)
    after.update(rewards=frame['rewards'][1:], actions=[(3, i) for i in range(4)] + [(6, None)], potion_count=1)
    history.after_step(frame, after, 0)
    frame = after
    assert history.mask(frame, [True] * 5) == ([True, True, True, True, False], 'card')
    for reward in (0, 1):
        history.before_step(frame, reward, [True] * 5)
        card = dict(frame, screen=6, rewards=[], actions=[(3, None), (7, None)])
        history.after_step(frame, card, reward)
        assert history.mask(card, [True, True]) == ([True, True], None)
        history.before_step(card, 1, [True, True])
        history.after_step(card, frame, 1)
    assert history.mask(frame, [True] * 5) == ([True] * 5, None)
    copy = deepcopy(history)
    copy.before_step(frame, 0, [True] * 5)
    card = dict(frame, screen=6, rewards=[], actions=[(3, None), (7, None)])
    copy.after_step(frame, card, 0)
    copy.before_step(card, 0, [True, True])
    taken = dict(frame, rewards=frame['rewards'][1:], actions=[(3, i) for i in range(3)] + [(6, None)], deck_size=13)
    copy.after_step(card, taken, 0)
    assert copy.mask(taken, [True] * 4) == ([True] * 4, None)
    assert history.counts['gold_appeared'] == history.counts['gold_taken'] == 1
    assert history.counts['potion_appeared'] == history.counts['potion_taken'] == 1
    assert history.counts['card_appeared'] == history.counts['card_opened'] == 2
    assert copy.counts['card_taken'] == 1
    assert history.mask(dict(frame, floor=2), [True] * 5)[1] == 'card'
    assert history.mask(dict(frame, screen=8), [True] * 5) == ([True] * 5, None)


def test_native_card_reward_skip_returns_once_per_floor():
    import pickle

    import torch

    from train.e4 import FLOOR, SCREEN, RoomSearch, SegmentCollector

    torch.set_num_threads(1)
    search = RoomSearch(1, 1, torch.device('cpu'), torch.Generator().manual_seed(1729), scratch_slots=2)
    collector = SegmentCollector(search, torch.Generator().manual_seed(10))
    collector.reset([0], [10])
    # Recorded full-run-10 prefix stops at real combat rewards containing gold and a card row.
    prefix = [0, 1, 0, 1, 9, 6, 2, 4, 7, 1, 4, 1, 0, 0, 2, 6, 6, 8, 3,
              0, 6, 2, 4, 0, 0, 2, 0, 6, 2, 0, 1, 3, 4, 3, 3, 1, 1]
    search.batch.reset_to(0, 10, prefix)
    search.provenance[0] = (10, prefix.copy())
    search.frame_cache.clear()
    search.reset_seen([0])
    assert search.progress()[0][SCREEN] == 7
    frame = search.frames([0], search.progress())[0]
    gold = next(a for a, (kind, reward) in enumerate(frame['actions'])
                if kind == 3 and reward is not None and frame['rewards'][reward][0] == 2)
    assert bool(search.policy_masks([0])[0, gold])
    search.step([0], [gold])
    target = search.loop_keys([0])[0]
    floor = search.progress()[0][FLOOR]
    frame = search.frames([0], search.progress())[0]
    card = next(a for a, (kind, reward) in enumerate(frame['actions'])
                if kind == 3 and reward is not None and frame['rewards'][reward][0] == 1)
    assert bool(search.policy_masks([0])[0, card])
    search.step([0], [card])
    assert search.progress()[0][SCREEN] == 6
    skip = next(a for a, (kind, _) in enumerate(search.frames([0], search.progress())[0]['actions']) if kind == 7)
    search.clone([0], [1])
    search.clone([0], [2])
    search.batch.step_slots([2], [skip])
    assert search.progress()[2][FLOOR] == floor
    assert search.loop_keys([2])[0] == target and target in search.seen[0]
    # The old repeat rule masks this real skip: its probe returns a same-floor seen key.
    raw = search.batch.state_bytes(0)
    assert bool(search.policy_masks([0])[0, skip])
    assert bool(search.policy_masks([0])[0, skip])
    search.mask_cache.pop(0)
    assert bool(search.policy_masks([0])[0, skip])
    assert search.batch.state_bytes(0) == raw and search.skip_returns[0] == set()
    assert search.loop_metrics['skip_repeat_exempt_card_reward'] == 0
    search.step([0], [skip])
    assert search.progress()[0][SCREEN] == 7 and search.loop_keys([0])[0] == target
    assert search.skip_returns[0] == {target} and search.skip_returns[1] == set()
    assert search.loop_metrics['skip_repeat_exempt_card_reward'] == 1
    counts = search.reward_history[0].counts
    assert counts['skip_repeat_exempt_card_reward'] == counts['card_skipped'] == counts['card_reward_choices'] == 1
    frame = search.frames([0], search.progress())[0]
    reopen = next(a for a, (kind, reward) in enumerate(frame['actions'])
                  if kind == 3 and reward is not None and frame['rewards'][reward][0] == 1)
    proceed = next(a for a, (kind, _) in enumerate(frame['actions']) if kind == 6)
    mask = search.policy_masks([0])[0]
    assert bool(mask[proceed]) and not bool(mask[reopen])
    collector.paths[0] = search.provenance[0][1].copy()
    saved = pickle.loads(pickle.dumps(collector.state()))
    search.clone([0], [2])
    assert search.skip_returns[2] == {target} and search.skip_returns[2] is not search.skip_returns[0]
    search.reset_seen([0])
    assert search.skip_returns[0] == set() and saved['skip_returns'][0] == {target}
    collector.restore(saved)
    assert search.batch.state_bytes(0) == search.batch.state_bytes(2)
    assert search.skip_returns[0] == {target}
    assert search.reward_history[0].counts['card_skipped'] == 1
    assert collector.pre_boss_deck_sizes == saved['pre_boss_deck_sizes']
    # Force the already masked reopen to exercise the second-return bound on native state.
    search.step([0], [reopen])
    assert search.progress()[0][SCREEN] == 6
    assert not bool(search.policy_masks([0])[0, skip])
    assert search.loop_metrics['skip_repeat_exempt_card_reward'] == 1
    collector.restore(saved)
    search.step([0], [proceed])
    assert search.progress()[0][SCREEN] == 9
    map_skip = next(a for a, (kind, _) in enumerate(search.frames([0], search.progress())[0]["actions"])
                    if kind == 7)
    assert not bool(search.policy_masks([0])[0, map_skip])
    assert search.loop_metrics["skip_repeat_exempt_map"] == 0
    assert search.reward_history[0].counts["skip_repeat_exempt_map"] == 0
    next_room = next(a for a, (kind, _) in enumerate(search.frames([0], search.progress())[0]['actions']) if kind == 3)
    search.step([0], [next_room])
    assert search.progress()[0][FLOOR] != floor and search.skip_returns[0] == set()
    collector.reset([0], [10])
    assert search.skip_returns[0] == set() and search.reward_history[0].counts == {}
