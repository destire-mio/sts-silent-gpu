"""Visible-state evaluation records; never used as policy inputs."""
from pathlib import Path
import math
import re
import torch
from train.e4 import ACT, FLOOR, PHASE, ROOM, SCREEN, HP, KEYS, COMBAT_PHASE, BOSS_ROOM, ELITE_ROOM, DEATH
from train.e4_stage12 import NoSLCollector


def catalog_names(kind):
    path = Path(__file__).resolve().parents[1] / 'core' / kind / 'catalog.def'
    return ['padding'] + re.findall(r'SPIRE_UNIT\([^,]+, "([^"]+)"', path.read_text())


CARDS, RELICS, POTIONS, MONSTERS = [catalog_names(kind) for kind in ('cards','relics','potions','monsters')]
RUNIC_DOME = RELICS.index('Runic Dome')


def visible_state(search, slot):
    count = int(search.arrays['entity_counts'][slot])
    tags = search.arrays['tags'][slot,:,:count].cpu().tolist()
    ids = search.arrays['ids'][slot,:,:count].cpu().tolist()
    values = search.arrays['values'][slot,:,:count].cpu().tolist()
    player = next(i for i in range(count) if tags[0][i] == 2)
    return dict(deck=[dict(card_id=CARDS[ids[0][i]],upgraded=bool(values[0][i]))
                      for i in range(count) if tags[0][i] == 3 and tags[1][i] == 1],
        relics=[RELICS[ids[0][i]] for i in range(count) if tags[0][i] == 6 and tags[1][i] == 0],
        potions=[POTIONS[ids[0][i]] for i in range(count) if tags[0][i] == 7 and tags[1][i] == 0],
        hp=values[0][player],max_hp=values[1][player],gold=values[2][player],
        encounter=[MONSTERS[ids[0][i]] for i in range(count) if tags[0][i] == 4])


def spearman(pairs):
    if len(pairs) < 2:
        return None
    def ranks(values):
        ordered = sorted(range(len(values)),key=values.__getitem__)
        result = [0.] * len(values)
        i = 0
        while i < len(ordered):
            j = i + 1
            while j < len(ordered) and values[ordered[j]] == values[ordered[i]]:
                j += 1
            for k in ordered[i:j]:result[k] = (i+j-1)/2
            i = j
        return result
    a,b = (ranks([p[j] for p in pairs]) for j in [0,1])
    mean = (len(a)-1)/2
    covariance = sum((x-mean)*(y-mean) for x,y in zip(a,b))
    denominator = math.sqrt(sum((x-mean)**2 for x in a)*sum((y-mean)**2 for y in b))
    return covariance/denominator if denominator else None


def value_correlations(episodes):
    result = {}
    for key in ['act1_boss_value','act2_entry_value']:
        pairs = [(e[key],e['final_floor']) for e in episodes if key in e]
        result[key] = dict(cases=len(pairs),spearman=spearman(pairs))
    return result


class EvaluationLedgerCollector(NoSLCollector):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.records = [None] * self.search.lanes
        self.turns = torch.empty(self.search.size,dtype=torch.int64,device=self.search.device)

    def reset(self,slots,seeds):
        super().reset(slots,seeds)
        for slot,seed in zip(slots,seeds):
            self.records[slot] = dict(seed=seed,act2_entry=None,counts=dict(card_skips=0,card_removals=0,
                rests=0,upgrades=0),elites_by_act=dict.fromkeys(['1','2','3','4'],0),
                elite_floors=[],combat_floors=[],room_entry_hp=None,encounter=[])

    def choose(self,model,slots,progress):
        actions,values,logs,imitation = super().choose(model,slots,progress)
        for slot,value in zip(slots,values):
            if slot in self.search.failed:continue
            record,p = self.records[slot],progress[slot]
            if p[ACT] == 1 and p[ROOM] == BOSS_ROOM and p[PHASE] == COMBAT_PHASE and 'act1_boss_value' not in record:
                record['act1_boss_value'] = value
            if p[ACT] == 2 and record['act2_entry'] is None:
                record['act2_entry'] = visible_state(self.search,slot)
                record['act2_entry_value'] = value
            if p[PHASE] == COMBAT_PHASE and p[FLOOR] not in record['combat_floors']:
                record['combat_floors'].append(p[FLOOR])
                record['encounter'] = visible_state(self.search,slot)['encounter']
                if record['room_entry_hp'] is None:record['room_entry_hp'] = p[HP]
                if p[ROOM] == ELITE_ROOM:
                    record['elites_by_act'][str(p[ACT])] += 1
                    record['elite_floors'].append(p[FLOOR])
        return actions,values,logs,imitation

    def advance(self,slots,actions,progress):
        active = [s for s in slots if s not in self.search.failed]
        frames = self.search.frames(active,progress) if active else {}
        for slot,action in zip(slots,actions):
            if slot not in frames:continue
            record,frame = self.records[slot],frames[slot]
            kind = frame['actions'][action][0]
            if frame['screen'] == 6 and kind == 7:record['counts']['card_skips'] += 1
            if frame['screen'] == 10 and kind == 3:
                option = frame['rest_options'][action]
                if option == 1:record['counts']['rests'] += 1
                if option == 2:record['counts']['upgrades'] += 1
        count = len(self.completed)
        result = super().advance(slots,actions,progress)
        after = dict(progress)
        after.update(self.search.last_step_progress)
        valid = [s for s in active if s not in self.search.failed]
        current_decks = (((self.search.arrays['tags'][valid,0] == 3) &
                         (self.search.arrays['tags'][valid,1] == 1)).sum(-1).cpu().tolist())
        self.search.batch.combat_turns_tensors(self.turns)
        for slot,deck in zip(valid,current_decks):
            record,p = self.records[slot],after[slot]
            record['counts']['card_removals'] += max(0,frames[slot]['deck_size']-deck)
            if p[FLOOR] != progress[slot][FLOOR]:
                record['room_entry_hp'] = progress[slot][HP]
                record['encounter'] = []
            if p[SCREEN] == DEATH:
                current = visible_state(self.search,slot)
                record['death'] = dict(room_type=p[ROOM],encounter=record['encounter'],
                    room_entry_hp=record['room_entry_hp'],remaining_potions=[p for p in current['potions'] if p != 'Potion Slot'],turn=int(self.turns[slot]))
        terminal_slots = [s for s,end in zip(slots,result[1]) if end]
        for episode,slot in zip(self.completed[count:],terminal_slots):
            record = self.records[slot]
            episode['keys'] = {name:bool(after[slot][KEYS] & bit)
                               for name,bit in [('ruby',1),('emerald',2),('sapphire',4)]}
            record['keys'] = episode['keys'].copy()
            record['reach'] = episode['reach'].copy()
            if episode['reach']['act3_second_boss'] and not episode['reach']['heart']:
                episode['outcome'] = 'act3_win'
                record.pop('death',None)
            record.update(final_floor=episode['floor'],outcome=episode['outcome'],stalled=episode['outcome']=='stalled')
            for key in ['elite_floors','combat_floors','room_entry_hp','encounter']:record.pop(key)
            episode['act2_ledger'] = record
        return result
