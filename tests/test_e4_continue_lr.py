import json
import sys
from copy import deepcopy
from types import SimpleNamespace
from unittest.mock import Mock

import pytest
import torch

from tools import e4_continue


def test_source_lr_override_preserves_adam_and_resume_requires_same_lr(tmp_path, monkeypatch):
    torch.set_num_threads(1)
    model = torch.nn.Linear(2, 1)
    adam = torch.optim.Adam
    optimizer = adam([{'params': [model.weight], 'lr': 3e-4},
                      {'params': [model.bias], 'lr': 2e-4}])
    model(torch.tensor([[1., 2.]])).square().sum().backward()
    optimizer.step()
    source = dict(commit='d28c060f92e38385670d70defad5a41fea3421a9', initialization=1729,
                  version=163, reason='before_evaluation', phase='evaluation', lanes=256,
                  trials=32, world_size=7, epochs=4, health_history=[], trained_seconds=1234.,
                  imitation_beta=.23, imitation_entropy_target=.61,
                  model=deepcopy(model.state_dict()), optimizer=deepcopy(optimizer.state_dict()),
                  ready=None, train_state={'partial': None, 'seeds': [1234567890]})
    source_dir = tmp_path / 'evaluation-163'
    source_dir.mkdir()
    torch.save(source, source_dir / 'rank-0.pt')
    baseline = dict(version=163, average_floor=19.2021484375,
                    reach=dict.fromkeys(e4_continue.REACH, .1))
    (tmp_path / 'E4-C53-163-N32-D1024-summary.json').write_text(json.dumps(baseline))
    output = tmp_path / 'output'
    monkeypatch.setenv('RANK', '7')
    monkeypatch.setenv('WORLD_SIZE', '8')
    monkeypatch.setenv('LOCAL_RANK', '7')
    monkeypatch.setattr(e4_continue, 'engine_commit', lambda: 'test-commit')
    monkeypatch.setattr(e4_continue, 'Policy', lambda config: torch.nn.Linear(2, 1))
    monkeypatch.setattr(e4_continue, 'DistributedDataParallel', lambda policy, **kwargs: policy)
    monkeypatch.setattr(e4_continue, 'dist', Mock())
    monkeypatch.setattr(e4_continue, 'waiting', lambda *args: True)
    cpu_torch = SimpleNamespace(**torch.__dict__)
    cpu_torch.device = lambda *args: torch.device('cpu')
    cpu_torch.cuda = SimpleNamespace(set_device=lambda device: None,
                                     get_rng_state=lambda device: torch.get_rng_state(),
                                     set_rng_state=lambda state, device: None)
    cpu_torch.optim = SimpleNamespace(Adam=lambda parameters, lr: adam(
        [{'params': [p]} for p in parameters], lr=lr))
    monkeypatch.setattr(e4_continue, 'torch', cpu_torch)

    def search(lanes, trials, device, rng, **kwargs):
        return SimpleNamespace(lanes=lanes, rng=rng, device=device,
                               map_returned=torch.zeros(lanes, dtype=torch.bool),
                               reward_history=[None] * lanes, seen={}, mask_cache={}, frame_cache={},
                               skip_returns={}, shop_skip_used={}, shop_opened={},
                               metrics={}, capacity_metrics={}, event_trial_max_steps=0,
                               loop_metrics={}, combat_diagnostics=None)

    monkeypatch.setattr(e4_continue, 'RoomSearch', search)
    command = ['e4_continue.py', '--source', str(source_dir), '--output', str(output),
               '--seconds', '300', '--lr', '1e-4']
    monkeypatch.setattr(sys, 'argv', command)
    e4_continue.main()
    saved = torch.load(output / 'rank-7.pt', weights_only=False)
    assert saved['lr'] == 1e-4
    assert saved['source_rank'] == 0 and saved['world_size'] == 8
    assert saved['source_version'] == saved['version'] == 163
    assert saved['imitation_beta'] == source['imitation_beta']
    assert saved['imitation_entropy_target'] == source['imitation_entropy_target']
    assert saved['train_state']['partial'] is None and saved['ready'] is None
    assert saved['train_state']['seeds'] == [None] * 256
    assert torch.equal(saved['train_state']['seed_rng'], torch.Generator().manual_seed(3736).get_state())
    assert saved['trained_seconds'] == 0 and saved['next_evaluation'] == 1800
    assert saved['best_floor'] == baseline['average_floor']
    for key, value in source['model'].items():
        assert torch.equal(saved['model'][key], value)
    assert len(saved['optimizer']['param_groups']) == 2
    assert all(group['lr'] == 1e-4 for group in saved['optimizer']['param_groups'])
    for key, state in source['optimizer']['state'].items():
        assert state['exp_avg'].abs().sum() > 0
        for field in ('step', 'exp_avg', 'exp_avg_sq'):
            assert torch.equal(saved['optimizer']['state'][key][field], state[field])
    monkeypatch.setattr(sys, 'argv', command + ['--resume', str(output)])
    e4_continue.main()
    resumed = torch.load(output / 'rank-7.pt', weights_only=False)
    assert torch.equal(resumed['ppo_rng'], saved['ppo_rng'])
    assert torch.equal(resumed['train_state']['seed_rng'], saved['train_state']['seed_rng'])
    monkeypatch.setattr(sys, 'argv', command[:-1] + ['3e-4', '--resume', str(output)])
    with pytest.raises(AssertionError):
        e4_continue.main()
