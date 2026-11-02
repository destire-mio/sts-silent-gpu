from copy import deepcopy
from types import SimpleNamespace
import torch
from train.e4 import ACT, FLOOR, PHASE, ROOM, SCREEN, HP, DEATH, REACH
from train.e4_stage12 import NoSLCollector
from train.e4_evaluation_ledger import EvaluationLedgerCollector
from tools.e4_stage12 import metrics


def test_keyless_act3_win_is_not_a_death(monkeypatch):
    episodes = [dict(seed=2000000064,floor=52,outcome='death',reward=.5,
                     reach=dict.fromkeys(REACH,False)),
                dict(seed=2000000065,floor=52,outcome='death',reward=.4,
                     reach=dict.fromkeys(REACH,False))]
    episodes[0]['reach']['act3_second_boss'] = True
    collector = object.__new__(EvaluationLedgerCollector)
    collector.completed = []
    collector.turns = torch.empty(2,dtype=torch.int64)
    collector.records = [dict(counts=dict(card_removals=0),elite_floors=[],combat_floors=[],
        room_entry_hp=50,encounter=[]) for _ in episodes]
    progress = {}
    for slot in range(2):
        p = [0] * (max(ACT,FLOOR,PHASE,ROOM,SCREEN,HP)+1)
        p[ACT],p[FLOOR],p[ROOM],p[SCREEN],p[HP] = 3,52,11,DEATH,50
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
    assert win['outcome'] == win['act2_ledger']['outcome'] == 'act3_win'
    assert 'death' not in win['act2_ledger'] and not win['act2_ledger']['stalled']
    assert death['outcome'] == death['act2_ledger']['outcome'] == 'death'
    assert death['act2_ledger']['death']['room_type'] == 11
    assert metrics(collector.completed)['victories'] == 1
    assert [e['reward'] for e in collector.completed] == [.5,.4]


def test_win_count_uses_reach_in_old_episode_records():
    episodes = [dict(floor=52,outcome='death',reach=dict.fromkeys(REACH,False)),
                dict(floor=52,outcome='victory',reach=dict.fromkeys(REACH,False))]
    episodes[0]['reach']['act3_second_boss'] = True
    assert metrics(episodes)['victories'] == 1
