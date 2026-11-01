from copy import deepcopy
import gzip
import json
from pathlib import Path
import torch
import spire
from train.e4 import RoomSearch
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
