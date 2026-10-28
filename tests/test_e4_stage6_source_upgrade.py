from copy import deepcopy
from datetime import timedelta
from types import SimpleNamespace

import pytest
import torch
import torch.distributed as dist

from train.e4_stage6.phase6_storage import Phase6Storage, file_digest


OLD = '89bd8c16d25b456212d5c8ad6c84615255fa4e41'
NEW = 'corrected-source-test'


@pytest.fixture
def bundle(tmp_path):
    dist.init_process_group('gloo', init_method=(tmp_path / 'gloo').as_uri(), rank=0, world_size=1,
                            timeout=timedelta(seconds=30))
    outer = dict(world=1, run_id='round0', runtime_commit=OLD,
                 source_hashes=['source-sha'] * 8, exam_sha256='exam', baseline_sha256='baseline',
                 evaluation_anchor_sha256='anchor')
    nested = dict(arm='A', rank=0, world=1, runtime_commit=OLD, source_commit='training-source',
                  source_sha256='source-sha', config={'width': 16}, source_update=8, source_Adam_step=512)
    payload = dict(binding=nested, model={'weight': torch.arange(3.)},
        optimizer={'state': {0: {'step': torch.tensor(512.), 'exp_avg': torch.ones(3)}}},
        rng=torch.Generator().manual_seed(1729).get_state(),
        collector={'seeds': [123], 'paths': [[1, 0]], 'combat_tape': {'groups': [torch.arange(2)]}},
        ready=torch.arange(4), cursor={'epoch': 2, 'offset': 8}, active_native_state_sha256={0: 'native'})
    old = Phase6Storage(tmp_path / 'output', outer)
    reference = old.save_worker(SimpleNamespace(binding=nested, snapshot=lambda: payload), 'training', 0)
    state = dict(schedule={'binding': deepcopy(outer), 'phase': 'training_B', 'round': 0},
                 last_matched={'A': reference, 'B': reference}, training={'A': reference},
                 evaluations={'A': {'complete': True}}, boundary={'A': 1.2, 'B': 1.3})
    old.save_state(state)
    current = dict(outer, runtime_commit=NEW)
    yield old, Phase6Storage(old.root, current), reference, payload, state
    dist.destroy_process_group()


def assert_payload_equal(actual, expected):
    if isinstance(expected, torch.Tensor):
        assert torch.equal(actual, expected)
    elif isinstance(expected, dict):
        assert actual.keys() == expected.keys()
        for key in expected:
            assert_payload_equal(actual[key], expected[key])
    elif isinstance(expected, list):
        assert len(actual) == len(expected)
        for left, right in zip(actual, expected):
            assert_payload_equal(left, right)
    else:
        assert actual == expected


def test_round0_source_upgrade_changes_only_binding_in_memory(bundle):
    old, current, reference, payload, state = bundle
    paths = [p for p in old.root.rglob('*') if p.is_file()]
    hashes = {p: (p.stat().st_size, file_digest(p)) for p in paths}
    restored = current.load_state()
    expected_state = deepcopy(state)
    expected_state['schedule']['binding']['runtime_commit'] = NEW
    assert restored == expected_state
    expected = deepcopy(payload)
    expected['binding']['runtime_commit'] = NEW
    assert_payload_equal(current.load_worker(reference), expected)
    assert hashes == {p: (p.stat().st_size, file_digest(p)) for p in paths}
    current.save_state(restored)
    assert current.load_state() == expected_state
    assert old.load_worker(reference)['binding']['runtime_commit'] == OLD


@pytest.mark.parametrize('field,value', [('run_id', 'another-run'),
    ('source_hashes', ['other']), ('exam_sha256', 'other'), ('baseline_sha256', 'other'),
    ('evaluation_anchor_sha256', 'other')])
def test_outer_identity_mismatch_rejects(bundle, field, value):
    old, current, reference, _, _ = bundle
    current.binding[field] = value
    with pytest.raises(AssertionError):
        current.load_state()
    with pytest.raises(AssertionError):
        current.load_worker(reference)


def test_unknown_saved_source_rejects(bundle):
    old, current, reference, payload, state = bundle
    old.binding['runtime_commit'] = 'unknown-source'
    old.save_state(state)
    with pytest.raises(AssertionError):
        current.load_state()
    payload['binding']['runtime_commit'] = 'unknown-source'
    ref = old.save_worker(SimpleNamespace(binding=payload['binding'], snapshot=lambda: payload), 'training', 0)
    with pytest.raises(AssertionError):
        current.load_worker(ref)


@pytest.mark.parametrize('target', ['manifest', 'rank-size', 'rank-hash'])
def test_original_integrity_checked_before_normalization(bundle, target):
    old, current, reference, _, _ = bundle
    current.resume_binding = lambda binding: pytest.fail('normalized before integrity checks')
    directory = old.root / reference['directory']
    path = directory / ('manifest.json' if target == 'manifest' else 'rank-0.pt')
    data = path.read_bytes()
    path.write_bytes(data + b'x' if target != 'rank-hash' else data[:-1] + bytes([data[-1] ^ 1]))
    with pytest.raises(AssertionError):
        current.load_worker(reference)


def test_nested_runtime_identity_and_schedule_mismatch_reject(bundle):
    old, current, reference, payload, state = bundle
    payload['binding']['runtime_commit'] = 'unknown-source'
    ref = old.save_worker(SimpleNamespace(binding=payload['binding'], snapshot=lambda: payload), 'training', 0)
    with pytest.raises(AssertionError):
        current.load_worker(ref)
    state['schedule']['binding']['run_id'] = 'other'
    old.save_state(state)
    with pytest.raises(AssertionError):
        current.load_state()
