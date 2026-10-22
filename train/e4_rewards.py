from collections import Counter
from dataclasses import dataclass, field
from difflib import SequenceMatcher


REWARDS = {1: 'card', 2: 'gold', 3: 'potion', 4: 'relic', 5: 'gold'}
REST_OPTIONS = ('rest', 'smith', 'lift', 'toke', 'dig', 'recall')


@dataclass
class RewardHistory:
    floor: int = -1
    rows: list = field(default_factory=list)
    opened: list = field(default_factory=list)
    active_card: int = None
    counts: Counter = field(default_factory=Counter)

    def observe(self, frame):
        if frame['floor'] != self.floor:
            self.floor = frame['floor']
            self.rows, self.opened, self.active_card = [], [], None
        if frame['screen'] != 7:
            return
        rows = [tuple(e) for e in frame['rewards']]
        opened = [False] * len(rows)
        for block in SequenceMatcher(a=self.rows, b=rows, autojunk=False).get_matching_blocks():
            for old, new in zip(range(block.a, block.a + block.size), range(block.b, block.b + block.size)):
                opened[new] = self.opened[old]
        previous = Counter(self.rows)
        for row in rows:
            if previous[row]:
                previous[row] -= 1
            elif row[0] in REWARDS:
                self.counts[REWARDS[row[0]] + '_appeared'] += 1
        self.rows, self.opened = rows, opened

    def mask(self, frame, legal):
        self.observe(frame)
        if frame['screen'] != 7:
            return legal, None
        for kind, rule in (((2, 5), 'gold'), ((3,), 'potion')):
            for a, (action, reward) in enumerate(frame['actions']):
                if (legal[a] and action == 3 and reward is not None
                        and self.rows[reward][0] in kind
                        and (rule != 'potion' or frame['empty_potion_slot'])):
                    return [i == a for i in range(len(legal))], rule
        if any(row[0] == 1 and not opened for row, opened in zip(self.rows, self.opened)):
            return [valid and frame['actions'][a][0] != 6 for a, valid in enumerate(legal)], 'card'
        return legal, None

    def before_step(self, frame, action, legal):
        _, rule = self.mask(frame, legal)
        if rule is not None:
            self.counts['mask_' + rule] += 1
        kind, reward = frame['actions'][action]
        if frame['screen'] == 7 and kind == 3:
            assert reward is not None
            if self.rows[reward][0] == 1:
                if not self.opened[reward]:
                    self.counts['card_opened'] += 1
                self.opened[reward] = True
                self.active_card = reward
        if frame['screen'] == 10 and kind == 3 and frame['rest_options'][action] is not None:
            self.counts['rest_' + REST_OPTIONS[frame['rest_options'][action] - 1]] += 1
        return kind, reward

    def after_step(self, before, after, action):
        kind, reward = before['actions'][action]
        if before['screen'] == 7 and kind == 3 and reward is not None:
            row = self.rows[reward]
            name = REWARDS.get(row[0])
            if name == 'gold' and after['gold'] > before['gold']:
                self.counts['gold_taken'] += 1
            if name == 'potion' and after['potion_count'] > before['potion_count']:
                self.counts['potion_taken'] += 1
            if name == 'relic' and after['relic_count'] > before['relic_count']:
                self.counts['relic_taken'] += 1
            if row[0] != 1 and (after['screen'] != 7 or len(after['rewards']) < len(self.rows)
                               or tuple(row) not in map(tuple, after['rewards'])):
                del self.rows[reward]
                del self.opened[reward]
        if before['screen'] == 6 and kind in (3, 7):
            self.counts['card_choices'] += 1
            self.counts['card_skipped'] += int(kind == 7)
        if before['screen'] == 6 and self.active_card is not None:
            if kind == 3:
                if after['deck_size'] > before['deck_size']:
                    self.counts['card_taken'] += 1
                else:
                    self.counts['card_bowl'] += 1
                del self.rows[self.active_card]
                del self.opened[self.active_card]
            if kind in (3, 7):
                self.active_card = None
        if before['screen'] == 11 and kind == 3 and after['gold'] < before['gold']:
            self.counts['shop_purchases'] += 1
        self.observe(after)


def noncombat_summary(episodes):
    counts = Counter({key: 0 for key in
        [name + suffix for name in ('gold', 'potion', 'card', 'relic') for suffix in ('_appeared', '_taken')]
        + ['card_opened', 'card_bowl', 'card_choices', 'card_skipped', 'shop_purchases', 'mask_gold', 'mask_potion', 'mask_card']
        + ['rest_' + name for name in REST_OPTIONS]})
    for episode in episodes:
        counts.update(episode['noncombat']['counts'])
    rest_choices = sum(counts['rest_' + name] for name in REST_OPTIONS)
    return dict(card_skip_rate=counts['card_skipped'] / counts['card_choices'] if counts['card_choices'] else None,
                rest_upgrade_rate=counts['rest_smith'] / rest_choices if rest_choices else None,
                boss_deck_size_mean={f'act{act}': counts[f'boss_act{act}_deck_size_sum'] / counts[f'boss_act{act}_arrivals']
                    if counts[f'boss_act{act}_arrivals'] else None for act in range(1, 5)},
                counts=dict(counts), final_deck_size_mean=sum(e['final_deck_size'] for e in episodes) / len(episodes),
                episodes=len(episodes), scope='actual trajectories; displayed combat/treasure reward rows')
