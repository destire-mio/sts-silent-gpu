from dataclasses import asdict
from hashlib import sha256
import json
from unittest.mock import patch

import pytest
import torch

from tools.e4_weight_edit import ACTION_KINDS, edit_kind_row, write_edit
from train.model import ModelConfig, Policy


def observation():
    generator = torch.Generator().manual_seed(701)
    batch, entities = 5, 27
    tags = torch.stack([torch.randint(n, (batch, entities), generator=generator)
                        for n in (13, 12, 256, 256)], dim=1)
    ids = torch.stack([torch.randint(n, (batch, entities), generator=generator)
                       for n in (512, 385, 385, 512)], dim=1)
    kinds = torch.arange(1, 8).repeat(batch, 2)
    mask = torch.ones_like(kinds, dtype=torch.bool)
    mask[:, -2:] = False
    entity_mask = torch.arange(entities)[None] < torch.tensor([27, 23, 17, 25, 19])[:, None]
    refs = torch.randint(0, 17, (batch, 3, kinds.shape[1]), generator=generator)
    refs[:, :, (kinds[0] == 2) | (kinds[0] == 6) | (kinds[0] == 7)] = 0
    return dict(tags=tags, ids=ids,
        values=torch.randint(-5, 200, (batch, 6, entities), generator=generator).float(),
        candidate_kinds=kinds, candidate_refs=refs, entity_mask=entity_mask, candidate_mask=mask)


@pytest.mark.parametrize('action_kind,strength', [('potion_use', 2.), ('potion_discard', -2.), ('end_turn', -.5)])
def test_edit_changes_only_target_logits_and_preserves_value(action_kind, strength):
    torch.set_num_threads(1)
    torch.manual_seed(611)
    policy = Policy(ModelConfig()).eval()
    before = {name: tensor.clone() for name, tensor in policy.state_dict().items()}
    with patch('torch.Tensor.backward', side_effect=AssertionError('No backward allowed')), \
            patch('torch.autograd.grad', side_effect=AssertionError('No gradients allowed')):
        edited, evidence = edit_kind_row(policy.state_dict(), action_kind, strength)
    modified = Policy(policy.config).eval()
    modified.load_state_dict(edited, strict=True)
    row = ACTION_KINDS.index(action_kind) + 1
    delta = edited['kind.weight'] != before['kind.weight']
    assert torch.equal(delta.any(dim=1), torch.arange(8) == row)
    assert evidence['changed_scalars'] == policy.config.width
    assert evidence['changed_tensors'] == ['kind.weight']
    assert all(torch.equal(before[name], tensor) for name, tensor in policy.state_dict().items())
    inputs = observation()
    with torch.no_grad():
        base_logits, base_value = policy(inputs)
        changed_logits, changed_value = modified(inputs)
    target = (inputs['candidate_kinds'] == row) & inputs['candidate_mask']
    assert torch.equal(base_logits[~target], changed_logits[~target])
    assert torch.equal(base_value, changed_value)
    assert (base_logits[target] != changed_logits[target]).any()
    assert torch.isneginf(changed_logits[~inputs['candidate_mask']]).all()


def test_saved_artifact_is_audited_evaluation_only_and_does_not_overwrite(tmp_path):
    torch.set_num_threads(1)
    torch.manual_seed(31)
    config = ModelConfig(width=32, layers=1, heads=4, feedforward=64)
    policy = Policy(config)
    checkpoint, output, manifest = [tmp_path / name for name in ('parent.pt', 'edited.pt', 'manifest.json')]
    source = dict(model=policy.state_dict(), config=dict(model=asdict(config)), engine_commit='abc123',
                  optimizer=dict(step=7), rng_states=[torch.get_rng_state()], update=7, curriculum=dict(band='k32'))
    torch.save(source, checkpoint)
    parent_bytes = checkpoint.read_bytes()
    result = write_edit(checkpoint, output, manifest, 'potion_use', 2.)
    saved = torch.load(output, map_location='cpu', weights_only=False)
    loaded = Policy(config)
    loaded.load_state_dict(saved['model'], strict=True)
    assert saved['evaluation_only'] is True and saved['artifact_type'] == 'weight_edit_evaluation'
    assert not {'optimizer', 'rng_states', 'update', 'curriculum', 'policy_version'} & saved.keys()
    assert saved['config'] == source['config'] and saved['engine_commit'] == source['engine_commit']
    assert checkpoint.read_bytes() == parent_bytes
    assert result == json.loads(manifest.read_text(encoding='utf-8'))
    assert result['parent_checkpoint_sha256'] == sha256(parent_bytes).hexdigest()
    assert result['output_sha256'] == sha256(output.read_bytes()).hexdigest()
    assert result['strict_reload_verified'] and result['parent_file_unchanged']
    assert result['gradients_computed'] == result['optimizer_steps'] == 0
    assert result['edit']['changed_scalars'] == config.width
    for name, row in result['edit']['tensor_evidence'].items():
        assert (row['before_sha256'] == row['after_sha256']) == (name != 'kind.weight')
    output_bytes, manifest_bytes = output.read_bytes(), manifest.read_bytes()
    with pytest.raises(AssertionError):
        write_edit(checkpoint, output, manifest, 'potion_use', 3.)
    with pytest.raises(AssertionError):
        write_edit(checkpoint, checkpoint, tmp_path / 'other.json', 'potion_use', 3.)
    assert output.read_bytes() == output_bytes and manifest.read_bytes() == manifest_bytes
    assert checkpoint.read_bytes() == parent_bytes
