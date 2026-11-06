from copy import deepcopy
import torch
from train.e4 import RewardLedger, SCREEN, PHASE, HP, ROOM, ACT, BATTLE_OVER, COMBAT_PHASE, BOSS_ROOM, DEATH
from train.e4_stage12 import NoSLCollector
from train.e4_lookahead import CardRewardLookahead, CardLookaheadSearch


class ArgmaxPolicy:
    def __call__(self, inputs):
        logits = (inputs['candidate_kinds'] == 2).float() * 8
        return logits, torch.zeros(len(logits))


def card_search():
    torch.set_num_threads(1)
    search = CardLookaheadSearch(1, torch.device('cpu'), 152)
    collector = NoSLCollector(search, torch.Generator().manual_seed(10))
    collector.reset([0], [10])
    prefix = [0,1,0,1,9,6,2,4,7,1,4,1,0,0,2,6,6,8,3,
              0,6,2,4,0,0,2,0,6,2,0,1,3,4,3,3,1,1]
    search.batch.reset_to(0, 10, prefix)
    search.provenance[0] = (10, prefix.copy())
    search.frame_cache.clear()
    search.reset_seen([0])
    progress = search.progress_for([0])
    assert progress[0][SCREEN] == 7
    frame = search.frames([0], progress)[0]
    card = next(a for a,(kind,reward) in enumerate(frame['actions'])
                if kind == 3 and reward is not None and frame['rewards'][reward][0] == 1)
    search.step([0], [card])
    assert search.progress_for([0])[0][SCREEN] == 6
    return search


def test_native_card_preview_is_paired_includes_skip_and_preserves_mainline():
    search = card_search()
    batch = search.batch
    paired = []
    class Recorded:
        def __getattr__(self, name):
            return getattr(batch, name)
        def determinize_slots(self, slots, seeds, run=False):
            assert run
            batch.determinize_slots(slots, seeds, run)
            paired.extend((seed,batch.state_bytes(slot)) for seed,slot in zip(seeds,slots))
    search.batch = Recorded()
    native = batch.state_bytes(0)
    rng = search.rng.get_state().clone()
    policy_rng = torch.random.get_rng_state().clone()
    path = deepcopy(search.provenance[0])
    ledger = deepcopy(search.reward_ledgers[0])
    history = deepcopy(search.reward_history[0])
    available = sorted(search.available)
    _, reports = search.lookahead.select(ArgmaxPolicy(), [0])
    report = reports[0]
    assert 0 < report['preview_steps'] <= 5*8*1024
    inputs = search.lookahead.inputs([0])
    legal = torch.where(inputs['candidate_mask'][0])[0].tolist()
    assert set(report['candidates']) == set(legal)
    assert any(int(inputs['candidate_kinds'][0,a]) == 7 for a in report['candidates'])
    assert len(report['seeds']) == 8
    for candidate in range(1,len(report['candidates'])):
        assert paired[:8] == paired[candidate*8:candidate*8+8]
    assert batch.state_bytes(0) == native and search.provenance[0] == path
    assert torch.equal(search.rng.get_state(), rng)
    assert torch.equal(torch.random.get_rng_state(), policy_rng)
    assert search.reward_ledgers[0] == ledger and search.reward_history[0] == history
    assert sorted(search.available) == available and search.failed == {}


def test_card_trigger_rejects_other_screen_single_action_and_confidence(monkeypatch):
    search = card_search()
    preview = search.lookahead
    class Policy:
        def __init__(self, logits):
            self.logits = torch.tensor([logits], dtype=torch.float32)
        def __call__(self, inputs):
            return self.logits, torch.zeros(1)
    def inputs(n):
        return dict(candidate_mask=torch.ones(1,n,dtype=torch.bool),
                    candidate_kinds=torch.tensor([[3]*(n-1)+[7]]))
    monkeypatch.setattr(preview, 'masked_inputs', lambda slots:inputs(1))
    assert preview.select(Policy([0]), [0])[1][0]['preview_steps'] == 0
    monkeypatch.setattr(preview, 'masked_inputs', lambda slots:inputs(2))
    assert preview.select(Policy([10,0]), [0])[1][0]['preview_steps'] == 0
    before = search.progress_for([0])[0]
    before[SCREEN] = 9
    monkeypatch.setattr(search, 'progress_for', lambda slots:{0:before})
    assert preview.select(Policy([0,0]), [0])[1][0]['preview_steps'] == 0


def test_card_predict_counts_only_card_decisions_and_never_combat_preview(monkeypatch):
    from train.e4_stage12 import NoSLSearch
    search = card_search()
    monkeypatch.setattr(NoSLSearch, 'predict', lambda self, model, slots:([0],[0.],[0.]))
    monkeypatch.setattr(search.lookahead, 'select', lambda model,slots:([1],[
        dict(preview_steps=40,candidates=[0,1],selected=1)]))
    assert search.predict(None,[0])[0] == [1]
    assert (search.card_reward_decisions,search.preview_decisions,search.overrides,search.preview_steps) == (1,1,1,40)
    progress = search.progress_for([0])[0]
    progress[SCREEN] = 0
    monkeypatch.setattr(search, 'progress_for', lambda slots:{0:progress})
    assert search.predict(None,[0])[0] == [0]
    assert search.mainline_decisions == 2 and search.preview_decisions == 1


def test_card_rollout_stops_at_three_combats_boss_death_or1024_steps():
    from types import SimpleNamespace
    for stop, expected in [('combats',6),('boss',2),('death',1),('limit',1024)]:
        row = [1,1,0,0,6,0,0,0,70]
        progress = {0:row}
        steps = {}
        search = SimpleNamespace(lanes=1, size=17, device=torch.device('cpu'),
            reward_ledgers=[RewardLedger()],available=list(range(1,17)),
            arrays=dict(faults=torch.zeros(17,dtype=torch.uint32),
                        truncated=torch.zeros(17,dtype=torch.bool)),
            clock=lambda:None)
        search.progress_for = lambda slots:{s:progress[s].copy() for s in slots}
        def take(count):
            result = search.available[-count:]
            del search.available[-count:]
            return result
        search.take = take
        def clone(sources, slots):
            for source,slot in zip(sources,slots):
                progress[slot] = progress[source].copy()
                steps[slot] = 0
        search.clone = clone
        search.batch = SimpleNamespace(determinize_slots=lambda slots,seeds,run:None)
        def step(slots, actions):
            for s in slots:
                steps[s] += 1
                p = progress[s]
                if stop == 'death':
                    p[SCREEN],p[HP] = DEATH,0
                elif stop in ('combats','boss'):
                    p[PHASE] = COMBAT_PHASE if steps[s] % 2 else 0
                    p[SCREEN] = 0 if steps[s] % 2 else 7
                    p[BATTLE_OVER] = int(not steps[s] % 2)
                    if stop == 'boss':
                        p[ROOM] = BOSS_ROOM
        search.step = step
        preview = CardRewardLookahead(search)
        def inputs(slots):
            return dict(candidate_mask=torch.ones(len(slots),2,dtype=torch.bool),
                        candidate_kinds=torch.tensor([[3,7]]*len(slots)))
        preview.masked_inputs = preview.inputs = inputs
        class Policy:
            def __call__(self, inputs):
                n = len(inputs['candidate_mask'])
                return torch.zeros(n,2),torch.ones(n)
        _,reports = preview.select(Policy(),[0])
        assert set(steps.values()) == {expected}
        assert reports[0]['preview_steps'] == 16*expected
        assert reports[0]['mean_scores'] == ([0.,0.] if stop == 'death' else
            [1+5/156,1+5/156] if stop == 'boss' else [1.,1.])
        assert search.available == list(range(1,17)) and progress[0] == row
