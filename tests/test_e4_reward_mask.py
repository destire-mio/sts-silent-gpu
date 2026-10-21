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
