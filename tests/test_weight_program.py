from dataclasses import asdict
from hashlib import sha256
from unittest.mock import patch

import torch

from tools.e4_weight_program import program_head, write_program
from train.model import ModelConfig, Policy


def inputs():
    torch.manual_seed(311)
    mask = torch.ones(3, 7, dtype=torch.bool)
    mask[1, -1] = False
    return dict(tags=torch.zeros(3, 4, 9, dtype=torch.long), ids=torch.zeros(3, 4, 9, dtype=torch.long),
        values=torch.randn(3, 6, 9) * 30, entity_mask=torch.ones(3, 9, dtype=torch.bool),
        candidate_kinds=torch.arange(1, 8).expand(3, -1),
        candidate_refs=torch.randint(0, 10, (3, 3, 7)), candidate_mask=mask)


def test_programmed_priors_are_fixed_offsets_and_keep_value_and_mask():
    torch.set_num_threads(1)
    torch.manual_seed(412)
    config = ModelConfig(width=32, layers=1, heads=4, feedforward=64)
    base = Policy(config).eval()
    before = {k: v.clone() for k, v in base.state_dict().items()}
    priors = [(3, 'end_turn', -.5), (9, 'skip', -.5), (11, 'proceed', .35)]
    with patch('torch.Tensor.backward', side_effect=AssertionError('No backward')), \
            patch('torch.autograd.grad', side_effect=AssertionError('No gradients')):
        changed = program_head(before, 1.5, priors)
        control = program_head(before, 1.5, [(u, k, 0.) for u, k, s in priors])
    programmed, removed = Policy(config).eval(), Policy(config).eval()
    programmed.load_state_dict(changed, strict=True)
    removed.load_state_dict(control, strict=True)
    obs = inputs()
    with torch.inference_mode():
        new, nv = programmed(obs)
        old, ov = removed(obs)
        _, bv = base(obs)
    expected = torch.tensor([0., -.5, 0., 0., 0., .35, -.5]).expand(3, -1)
    legal = obs['candidate_mask']
    torch.testing.assert_close((new - old)[legal], expected[legal], atol=2e-5, rtol=0)
    assert torch.equal(nv, ov) and torch.equal(nv, bv)
    assert torch.isneginf(new[~legal]).all()
    assert all(torch.equal(before[k], v) for k, v in base.state_dict().items())
    allowed = {'score.0.weight', 'score.0.bias', 'score.2.weight'}
    assert all(torch.equal(v, changed[k]) for k, v in before.items() if k not in allowed)


def test_saved_program_preserves_parent_and_drops_resume_state(tmp_path):
    torch.set_num_threads(1)
    config = ModelConfig(width=32, layers=1, heads=4, feedforward=64)
    parent, output, manifest = [tmp_path / name for name in ('parent.pt', 'program.pt', 'manifest.json')]
    torch.save(dict(model=Policy(config).state_dict(), config=dict(model=asdict(config)),
                    engine_commit='abc123', optimizer={'step': 99}, update=99), parent)
    identity = sha256(parent.read_bytes()).hexdigest()
    result = write_program(parent, output, manifest, 1.5, [(3, 'end_turn', -.5)])
    saved = torch.load(output, weights_only=True)
    Policy(config).load_state_dict(saved['model'], strict=True)
    assert saved['evaluation_only'] and 'optimizer' not in saved and 'update' not in saved
    assert result['source_unchanged'] and result['strict_reload_verified']
    assert identity == sha256(parent.read_bytes()).hexdigest() == result['parent_sha256']
    assert result['gradients'] == result['optimizer_steps'] == 0
