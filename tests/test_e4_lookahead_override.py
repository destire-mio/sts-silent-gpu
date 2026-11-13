from types import SimpleNamespace

import torch

from train.e4_lookahead import CombatLookahead


def test_combat_override_requires_two_differences_and_positive_validation(monkeypatch):
    search = SimpleNamespace(size=33, device=torch.device('cpu'), lanes=1, available=list(range(1, 33)),
                             arrays={'tags': torch.zeros(1, 2, 1), 'ids': torch.zeros(1, 1, 1)})

    def policy(inputs):
        return torch.tensor([[1., 0.]]), None

    def run(first_scores, validation_scores=None, **options):
        lookahead = CombatLookahead(search, **options)
        batches = []
        monkeypatch.setattr(lookahead, 'masked_inputs',
                            lambda slots: {'candidate_mask': torch.ones(1, 2, dtype=torch.bool)})

        def rollout(policy, sources, actions, seeds):
            batches.append((actions, seeds))
            scores = first_scores if len(batches) == 1 else validation_scores
            assert scores is not None and len(scores) == len(actions)
            return scores, [1] * len(scores)

        monkeypatch.setattr(lookahead, 'rollout', rollout)
        actions, reports = lookahead.select(policy, [0])
        return actions, reports[0], batches

    singleton = [0.] * 8 + [8.] + [0.] * 7
    for multiplier in (2.0, 0.5):
        actions, report, batches = run(singleton, override_se=multiplier)
        assert actions == [0] and len(batches) == 1 and 'validated' not in report

    promising = [0.] * 8 + [1.] * 8
    actions, report, batches = run(promising)
    assert actions == [1] and len(batches) == 1 and 'validated' not in report

    actions, report, batches = run(promising, [1.] * 4 + [0.] * 4, override_se=2.0, validate=4)
    assert actions == [0] and report['selected'] == 0
    assert report['validated'] is False
    assert report['validated_delta'] == -1. and report['validated_se'] == 0.
    assert report['validated_k'] == 4 and report['preview_steps'] == 24
    second_actions, seeds = batches[1]
    assert second_actions == [0] * 4 + [1] * 4 and seeds[:4] == seeds[4:]
    assert len(set(seeds[:4])) == 4 and not set(seeds).intersection(batches[0][1])

    actions, report, batches = run(promising, [0.] * 4 + [1.] * 4, override_se=2.0, validate=4)
    assert actions == [1] and report['validated'] is True and report['validated_delta'] == 1.
