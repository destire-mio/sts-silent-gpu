import torch

from tools.e4_search_exam import evaluate, parse_args
from train.e4_lookahead import CombatLookahead
from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig


def test_combined_cpu_search_exam_uses_sixteen_paired_draws(monkeypatch, tmp_path):
    torch.set_num_threads(1)
    torch.manual_seed(130)
    device = torch.device('cpu')
    policy = CLSPolicy(ModelConfig(16, 1, 2, 32)).eval()
    args = parse_args(['--model', str(tmp_path / 'policy.pt'), '--output', str(tmp_path / 'result.json'),
                       '--episodes', '2', '--lanes', '2', '--combat-draws', '16',
                       '--cards', 'on', '--map-k', '2'])
    rollout = CombatLookahead.rollout
    initial_batches = []

    def check_paired_draws(self, model, sources, actions, seeds):
        assert len(actions) == len(sources) == len(seeds)
        for source in set(sources):
            by_action = {}
            for parent, action, seed in zip(sources, actions, seeds):
                if parent == source:
                    by_action.setdefault(action, []).append(seed)
            draws = next(iter(by_action.values()))
            assert len(draws) == len(set(draws)) == 16
            assert len(by_action) > 1 and all(values == draws for values in by_action.values())
        initial_batches.append(len(seeds))
        return rollout(self, model, sources, actions, seeds)

    monkeypatch.setattr(CombatLookahead, 'rollout', check_paired_draws)
    result = evaluate(policy, args, device)
    assert {row['seed'] for row in result['episodes']} == {2000000000, 2000000001}
    assert all(row['outcome'] in ('death', 'victory') and 'floor' in row and 'reach' in row
               for row in result['episodes']), result['episodes']
    assert initial_batches
    assert all(stats['preview_decisions'] > 0 and stats['preview_steps'] > 0
               for stats in result['search'].values()), result['search']
    assert not result['key_last_chance']
