from copy import deepcopy
import gzip
import json
import pytest
from pathlib import Path
import torch
import spire
from train.e4 import RoomSearch, RewardLedger, PHASE, COMBAT_PHASE, SCREEN, BATTLE_OVER, HP
from train.e4_stage6.examination import load_roots
from train.e4_stage12 import CLSPolicy, NoSLSearch
from train.e4_lookahead import CombatLookahead
from train.model import ModelConfig


def setup():
    torch.set_num_threads(1)
    torch.manual_seed(130)
    search = NoSLSearch(2, torch.device('cpu'), 130, storage_slots=64, greedy=True)
    path = Path(__file__).resolve().parents[1] / 'inventory/e4-c88-stage6-combat-exam.json.gz'
    with gzip.open(path, 'rt') as stream:
        scene = json.load(stream)['groups']['act1_boss_guardian'][0]
    load_roots(search, [scene, scene])
    policy = CLSPolicy(ModelConfig(16,1,2,32)).eval()
    search.reward_ledgers = [RewardLedger(),RewardLedger()]
    return search, policy


def test_preview_preserves_real_slot_and_all_rng(monkeypatch):
    search, policy = setup()
    before = [search.batch.state_bytes(s) for s in [0,1]]
    rng = search.rng.get_state().clone()
    metadata = deepcopy([search.provenance[s] for s in [0,1]])
    mask_calls = []
    original_mask = search.policy_masks
    def mask(slots):
        result = original_mask(slots)
        for row in result:
            legal = torch.where(row)[0]
            if len(legal) > 1: row[legal[0]] = False
        mask_calls.append(len(slots))
        return result
    monkeypatch.setattr(search, 'policy_masks', mask)
    for depth in ('turn', 'combat'):
        _, reports = CombatLookahead(search, depth=depth).select(policy, [0,1])
        assert all(0 < r['preview_steps'] <= 32 * (64 if depth == 'turn' else 256) for r in reports)
        assert [search.batch.state_bytes(s) for s in [0,1]] == before
        assert torch.equal(search.rng.get_state(), rng)
        assert [search.provenance[s] for s in [0,1]] == metadata
        assert max(mask_calls) > 32
        assert all(0 not in r['candidates'] for r in reports)


def test_eight_hidden_draw_orders_differ():
    scenario = dict(deck=[dict(id=name,upgrades=0) for name in
        ['Strike_G','Defend_G','Neutralize','Survivor','Backflip','Dagger Throw','Deadly Poison',
         'Cloak And Dagger','Prepared','Slice','Bane','Dodge and Roll','Flying Knee','Quick Slash',
         'Poisoned Stab','Sucker Punch','Dash','Leg Sweep','Blur','Footwork']],
        relics=[],potions=[],hp=70,max_hp=70,encounter='Cultist')
    game = spire.start_combat(130, scenario)
    orders = []
    for seed in range(8):
        copy = game.copy()
        copy.determinize(seed)
        snapshot = copy.snapshot()
        player = snapshot['objects'][snapshot['player']['ref']]['fields']
        group = player['AbstractPlayer.drawPile']['ref']
        cards = snapshot['objects'][group]['fields']['CardGroup.group']
        orders.append(tuple(snapshot['objects'][c['ref']]['fields']['AbstractCard.cardID'] for c in cards))
    assert len(set(orders)) == 8


def test_single_action_and_confident_policy_skip_preview(monkeypatch):
    search, policy = setup()
    preview = CombatLookahead(search)
    def inputs(slots):
        return dict(candidate_mask=torch.ones(len(slots), 1, dtype=torch.bool))
    monkeypatch.setattr(preview, 'masked_inputs', inputs)
    class Single:
        def __call__(self, inputs):
            return torch.zeros(1,1), torch.zeros(1)
    assert preview.select(Single(), [0])[1][0]['preview_steps'] == 0
    monkeypatch.setattr(preview, 'masked_inputs', lambda slots:dict(candidate_mask=torch.ones(1,2,dtype=torch.bool)))
    class Confident:
        def __call__(self, inputs):
            return torch.tensor([[10.,0.]]), torch.zeros(1)
    assert preview.select(Confident(), [0])[1][0]['preview_steps'] == 0

    from train.e4_evaluation_ledger import RUNIC_DOME
    search,policy = setup()
    search.arrays = dict(search.arrays)
    for key in ['tags','ids']:search.arrays[key] = search.arrays[key].clone()
    search.arrays['tags'][0,0,0] = 6
    search.arrays['tags'][0,1,0] = 0
    search.arrays['ids'][0,0,0] = RUNIC_DOME
    _,reports = CombatLookahead(search).select(policy,[0])
    assert reports[0]['preview_steps'] == 0 and reports[0]['skipped'] == 'Runic Dome'


def test_paired_k_has_identical_hidden_state_across_candidates():
    search, policy = setup()
    batch = search.batch
    recorded = []
    class Recorded:
        def __getattr__(self, name):
            return getattr(batch, name)
        def determinize_slots(self, slots, seeds):
            batch.determinize_slots(slots, seeds)
            recorded.extend(batch.state_bytes(s) for s in slots)
    search.batch = Recorded()
    _, reports = CombatLookahead(search).select(policy, [0,1])
    report = reports[0]
    assert len(recorded) == sum(8 * len(r['candidates']) for r in reports)
    for group in range(1, len(report['candidates'])):
        assert recorded[:8] == recorded[group*8:group*8+8]


def test_boss_kill_preview_includes_reward_and_preserves_source_ledger(monkeypatch):
    import pytest
    search,policy = setup()
    before = search.progress_for([0,1])
    original = deepcopy(search.reward_ledgers)
    done = set()
    batch = search.batch
    class BossKill:
        def __getattr__(self,name):return getattr(batch,name)
        def step_slots(self,slots,actions):done.update(slots)
    search.batch = BossKill()
    def progress(slots):
        result = {}
        for slot in slots:
            p = list(before[0])
            if slot in done:
                p[PHASE],p[SCREEN],p[BATTLE_OVER],p[HP] = COMBAT_PHASE+1,7,1,70
            result[slot] = p
        return result
    monkeypatch.setattr(search,'progress_for',progress)
    class ZeroValue:
        def __call__(self,inputs):
            return torch.zeros_like(inputs['candidate_mask'],dtype=torch.float32),torch.zeros(len(inputs['candidate_mask']))
    _,reports = CombatLookahead(search).select(ZeroValue(),[0])
    assert reports[0]['mean_scores']
    assert all(score == pytest.approx(5/156) for score in reports[0]['mean_scores'])
    assert search.reward_ledgers == original


@pytest.mark.parametrize('fault', [0, 852769260])
def test_combat_preview_mid_step_fault_keeps_argmax_and_records_replay(tmp_path, monkeypatch, fault):
    search, _ = setup()
    search.evidence_directory = tmp_path
    batch = search.batch
    source_seed, prefix = search.provenance_record(0)
    preview = CombatLookahead(search, depth='combat')
    origin = {}
    taken = {}
    calls = []
    hit = []

    class Batch:
        def __getattr__(self, name):
            return getattr(batch, name)

        def determinize_slots(self, slots, seeds):
            batch.determinize_slots(slots, seeds)
            origin.update(zip(slots, seeds))

        def step_slots(self, slots, actions):
            calls.append(list(slots))
            if len(calls) == 1:
                search.arrays['entity_counts'][slots] = 10
                search.arrays['faults'].view(torch.int32)[slots] = 0
            for slot, action in zip(slots, actions):
                taken.setdefault(slot, []).append(action)
            if len(calls) == 2:
                hit.append(slots[0])
                search.arrays['faults'][slots[0]] = fault
                if not fault:
                    search.arrays['entity_counts'][slots[0]] = 0

    search.batch = Batch()
    search.arrays = dict(search.arrays)
    for key in ('faults', 'entity_counts'):
        search.arrays[key] = search.arrays[key].clone()
    initial_progress = search.progress_for([0])[0]
    initial_progress[PHASE], initial_progress[SCREEN], initial_progress[HP] = COMBAT_PHASE, 0, 70

    def progress(slots):
        results = {slot: initial_progress.copy() for slot in slots}
        if len(calls) >= 3:
            for row in results.values():
                row[PHASE] = COMBAT_PHASE + 1
        return results

    monkeypatch.setattr(search, 'progress_for', progress)
    def inputs(slots):
        assert not set(hit).intersection(slots)
        return dict(candidate_mask=torch.ones(len(slots), 2, dtype=torch.bool))

    monkeypatch.setattr(preview, 'masked_inputs', inputs)
    monkeypatch.setattr(preview, 'inputs', inputs)

    class Policy:
        def __call__(self, inputs):
            n = len(inputs['candidate_mask'])
            return torch.tensor([[0., 1.]]).repeat(n, 1), torch.full((n,), 3.)

    available = search.available.copy()
    chosen, reports = preview.select(Policy(), [0])
    assert chosen == [1] and reports[0]['skipped'] == 'combat preview fault'
    assert search.available == available and not search.failed
    slot = hit[0]
    assert slot not in calls[2]
    path = tmp_path / 'combat-preview-first-fault.json'
    raw = path.read_bytes()
    row = json.loads(raw)
    assert row['event'] == 'episode_fault' and row['scope'] == 'combat_preview'
    assert row['seed'] == source_seed and row['fault'] == fault
    assert (row['entity_count'] > 0) == bool(fault)
    assert row['reason'] == (f'engine_fault:{fault}' if fault else 'empty_preview_entities')
    assert row['candidates'] == prefix + taken[slot]
    assert len(taken[slot]) == 2
    assert row['randomizations'] == [[len(prefix), origin[slot], False]]
    preview.record_preview_fault(0, slot, 2, [], 1, 0)
    assert path.read_bytes() == raw
