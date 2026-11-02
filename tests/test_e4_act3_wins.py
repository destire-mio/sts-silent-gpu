from copy import deepcopy
from types import SimpleNamespace
import pytest
import torch
from train.e4 import ACT, FLOOR, PHASE, ROOM, SCREEN, HP, DEATH, VICTORY, REACH
from train.e4_stage12 import NoSLCollector
from train.e4_evaluation_ledger import EvaluationLedgerCollector
from tools.e4_stage12 import metrics


@pytest.mark.parametrize('heart_win',[False,True])
def test_act3_and_heart_win_outcomes(monkeypatch,heart_win):
    episodes = [dict(seed=2000000064,floor=52,outcome='death',reward=.5,
                     reach=dict.fromkeys(REACH,False)),
                dict(seed=2000000065,floor=52,outcome='death',reward=.4,
                     reach=dict.fromkeys(REACH,False))]
    episodes[0]['reach']['act3_second_boss'] = True
    episodes[0]['reach']['heart'] = heart_win
    if heart_win:episodes[0]['outcome'] = 'victory'
    collector = object.__new__(EvaluationLedgerCollector)
    collector.completed = []
    collector.turns = torch.empty(2,dtype=torch.int64)
    collector.records = [dict(counts=dict(card_removals=0),elite_floors=[],combat_floors=[],
        room_entry_hp=50,encounter=[]) for _ in episodes]
    progress = {}
    for slot in range(2):
        p = [0] * (max(ACT,FLOOR,PHASE,ROOM,SCREEN,HP)+1)
        p[ACT],p[FLOOR],p[ROOM],p[SCREEN],p[HP] = 3,52,11,DEATH,50
        if heart_win and slot == 0:p[SCREEN] = VICTORY
        progress[slot] = p
    collector.search = SimpleNamespace(failed={},last_step_progress=progress,
        arrays={'tags':torch.tensor([[[3],[1]],[[3],[1]]])},
        frames=lambda slots,progress:{s:dict(screen=0,deck_size=1,actions=[(0,None)]) for s in slots},
        batch=SimpleNamespace(combat_turns_tensors=lambda turns:turns.fill_(1)))
    def advance(self,slots,actions,progress):
        self.completed.extend(deepcopy(episodes))
        return [.1,.2],[True,True]
    monkeypatch.setattr(NoSLCollector,'advance',advance)
    monkeypatch.setattr('train.e4_evaluation_ledger.visible_state',
                        lambda search,slot:dict(potions=['Potion Slot']))
    rewards,terminal = collector.advance([0,1],[0,0],progress)
    win,death = collector.completed
    assert rewards == [.1,.2] and terminal == [True,True]
    assert win['outcome'] == win['act2_ledger']['outcome'] == ('victory' if heart_win else 'act3_win')
    assert 'death' not in win['act2_ledger'] and not win['act2_ledger']['stalled']
    assert death['outcome'] == death['act2_ledger']['outcome'] == 'death'
    assert death['act2_ledger']['death']['room_type'] == 11
    assert metrics(collector.completed)['victories'] == 1
    assert [e['reward'] for e in collector.completed] == [.5,.4]


def test_win_count_uses_reach_in_old_episode_records():
    episodes = [dict(floor=52,outcome='death',reach=dict.fromkeys(REACH,False)),
                dict(floor=52,outcome='victory',reach=dict.fromkeys(REACH,False))]
    episodes[0]['reach']['act3_second_boss'] = True
    for e in episodes:e['keys'] = dict(ruby=False,emerald=False,sapphire=False)
    assert metrics(episodes)['victories'] == 1


def test_key_and_act4_heart_columns():
    episodes = []
    for i,bits in enumerate([7,1,5,2]):
        reach = dict.fromkeys(REACH,False)
        reach.update(act3_second_boss=i<3,act4=i<2,shield_spear=i==0,heart=i==0)
        episodes.append(dict(floor=57,outcome='victory' if i==0 else 'death',reach=reach,
            keys={name:bool(bits & bit) for name,bit in [('ruby',1),('emerald',2),('sapphire',4)]}))
    row = metrics(episodes)
    assert row['key_counts'] == dict(ruby=3,emerald=2,sapphire=2)
    assert row['all_three_keys'] == 1 and row['act4_entries'] == 2
    assert row['shield_spear_wins'] == row['heart_wins'] == 1
    assert row['victories'] == 3
