from hashlib import sha256
import json
from unittest.mock import patch

import pytest
import torch

from tools.e4_weight_edit import ACTION_KINDS, BLOCKS, MODEL_CONFIG, edit_block, edit_kind_row, write_edit
from train.e4_stage12 import CLSPolicy
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
    policy = CLSPolicy(ModelConfig(width=32, layers=2, heads=4, feedforward=64)).eval()
    before = {name: tensor.clone() for name, tensor in policy.state_dict().items()}
    with patch('torch.Tensor.backward', side_effect=AssertionError('No backward allowed')), \
            patch('torch.autograd.grad', side_effect=AssertionError('No gradients allowed')):
        edited, evidence = edit_kind_row(policy.state_dict(), action_kind, strength)
    modified = CLSPolicy(policy.config).eval()
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


@pytest.mark.parametrize('block', BLOCKS)
def test_block_edit_preserves_encoder_value_and_other_weight_columns(block):
    torch.set_num_threads(1)
    torch.manual_seed(42)
    policy = CLSPolicy(ModelConfig(width=32, layers=2, heads=4, feedforward=64)).eval()
    before = {key: value.clone() for key, value in policy.state_dict().items()}
    edited, evidence = edit_block(before, block, 1.1)
    width = policy.config.width
    changed = edited['score.0.weight'] != before['score.0.weight']
    expected = torch.zeros_like(changed)
    expected[:, BLOCKS[block] * width:(BLOCKS[block] + 1) * width] = True
    assert torch.equal(changed, expected)
    assert evidence['changed_scalars'] == width * width
    assert all(torch.equal(value, edited[key]) for key, value in before.items() if key != 'score.0.weight')
    modified = CLSPolicy(policy.config).eval()
    modified.load_state_dict(edited, strict=True)
    inputs = observation()
    with torch.no_grad():
        a, av = policy(inputs)
        b, bv = modified(inputs)
    assert torch.equal(av, bv)
    assert (a[inputs['candidate_mask']] != b[inputs['candidate_mask']]).any()
    assert torch.isneginf(b[~inputs['candidate_mask']]).all()
    assert all(torch.equal(value, policy.state_dict()[key]) for key, value in before.items())


def test_saved_raw_64h_artifact_is_audited_and_does_not_overwrite(tmp_path):
    torch.set_num_threads(1)
    torch.manual_seed(31)
    policy = CLSPolicy(MODEL_CONFIG)
    checkpoint, output, manifest = [tmp_path / name for name in ('parent.pt', 'edited.pt', 'manifest.json')]
    source = policy.state_dict()
    torch.save(source, checkpoint)
    parent_bytes = checkpoint.read_bytes()
    identity = sha256(parent_bytes).hexdigest()
    with patch('torch.load', side_effect=AssertionError('Must not deserialize the wrong parent')):
        with pytest.raises(AssertionError, match='SHA256'):
            write_edit(checkpoint, output, manifest, expected_sha256='0' * 64, block='context', scale=.9)
    assert not output.exists() and not manifest.exists()
    assert checkpoint.read_bytes() == parent_bytes
    result = write_edit(checkpoint, output, manifest, expected_sha256=identity, block='context', scale=.9)
    saved = torch.load(output, map_location='cpu', weights_only=True)
    loaded = CLSPolicy(MODEL_CONFIG)
    loaded.load_state_dict(saved, strict=True)
    assert result['evaluation_only'] is True and result['raw_state_dict'] is True
    assert all(isinstance(value, torch.Tensor) for value in saved.values())
    assert 'cls' in saved and sum(value.numel() for value in saved.values()) == 12435074
    assert not {'optimizer', 'rng_states', 'update', 'curriculum', 'policy_version'} & saved.keys()
    assert checkpoint.read_bytes() == parent_bytes
    assert result == json.loads(manifest.read_text(encoding='utf-8'))
    assert result['parent_checkpoint_sha256'] == sha256(parent_bytes).hexdigest()
    assert result['output_sha256'] == sha256(output.read_bytes()).hexdigest()
    assert result['strict_reload_verified'] and result['parent_file_unchanged']
    assert result['gradients_computed'] == result['optimizer_steps'] == 0
    assert result['edit']['changed_scalars'] == MODEL_CONFIG.width ** 2
    for name, row in result['edit']['tensor_evidence'].items():
        assert (row['before_sha256'] == row['after_sha256']) == (name != 'score.0.weight')
    output_bytes, manifest_bytes = output.read_bytes(), manifest.read_bytes()
    with pytest.raises(AssertionError):
        write_edit(checkpoint, output, manifest, expected_sha256=identity, action_kind='potion_use', strength=.5)
    with pytest.raises(AssertionError):
        write_edit(checkpoint, checkpoint, tmp_path / 'other.json', expected_sha256=identity,
                   action_kind='potion_use', strength=.5)
    assert output.read_bytes() == output_bytes and manifest.read_bytes() == manifest_bytes
    assert checkpoint.read_bytes() == parent_bytes


@pytest.mark.parametrize('kind', ['wrapped_e3', 'raw_e3', 'wrong_cls_shape'])
def test_writer_rejects_e3_and_wrong_cls_architecture(tmp_path, kind):
    torch.set_num_threads(1)
    config = ModelConfig(width=32, layers=1, heads=4, feedforward=64)
    source = (CLSPolicy(config) if kind == 'wrong_cls_shape' else Policy(config)).state_dict()
    if kind == 'wrapped_e3':
        source = dict(model=source, config=dict(model=dict(width=32, layers=1, heads=4, feedforward=64)))
    checkpoint, output, manifest = [tmp_path / name for name in ('wrong.pt', 'edited.pt', 'manifest.json')]
    torch.save(source, checkpoint)
    original = checkpoint.read_bytes()
    with pytest.raises((AssertionError, RuntimeError)):
        write_edit(checkpoint, output, manifest, expected_sha256=sha256(original).hexdigest(),
                   action_kind='potion_use', strength=.5)
    assert checkpoint.read_bytes() == original
    assert not output.exists() and not manifest.exists()
