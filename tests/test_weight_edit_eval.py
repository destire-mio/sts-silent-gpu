from copy import deepcopy
from hashlib import sha256
from unittest.mock import patch

import pytest
import torch

from tools.e4_weight_edit_eval import evaluation_receipt, read_state_dict


def test_checkpoint_identity_rejects_wrong_hash_before_loading(tmp_path):
    path = tmp_path / 'model.pt'
    torch.save({'cls': torch.zeros(1, 1, 384)}, path)
    with patch('torch.load', side_effect=AssertionError('must not deserialize')):
        with pytest.raises(AssertionError, match='SHA256'):
            read_state_dict(path, '0' * 64)
    state = read_state_dict(path, sha256(path.read_bytes()).hexdigest())
    assert state['cls'].shape == (1, 1, 384)


def test_checkpoint_requires_raw_finite_cls_state(tmp_path):
    path = tmp_path / 'model.pt'
    torch.save({'model': {'cls': torch.zeros(1, 1, 384)}}, path)
    with pytest.raises(AssertionError, match='raw CLS'):
        read_state_dict(path, sha256(path.read_bytes()).hexdigest())
    torch.save({'cls': torch.full((1, 1, 384), float('nan'))}, path)
    with pytest.raises(AssertionError, match='Nonfinite'):
        read_state_dict(path, sha256(path.read_bytes()).hexdigest())


def test_receipt_keeps_stalls_faults_and_unfinished_runs_separate_from_deaths():
    reach = dict.fromkeys(('act1_boss', 'act2_boss', 'act3_first_boss', 'act3_second_boss',
                          'act4', 'shield_spear', 'heart'), False)
    row = dict(seed=2000000000, floor=5, outcome='death', reach=reach,
               keys=dict.fromkeys(('ruby', 'emerald', 'sapphire'), False), stall_class=None)
    stalled, won = deepcopy(row), deepcopy(row)
    stalled.update(seed=2000000001, outcome='stalled', stall_class='engine_fault:123')
    won.update(seed=2000000002, floor=51, outcome='act3_win')
    won['reach']['act3_second_boss'] = True
    seeds = list(range(2000000000, 2000000004))
    result = evaluation_receipt([won, stalled, row], seeds, False)
    assert result['unfinished_seeds'] == [2000000003]
    assert result['summary']['outcomes'] == {'death': 1, 'stalled': 1, 'act3_win': 1}
    assert result['summary']['unresolved_stalls'] == 1
    assert result['summary']['stall_classes'] == {'engine_fault:123': 1}
    assert result['summary']['act3_wins'] == 1 and result['summary']['heart_wins'] == 0
    with pytest.raises(AssertionError, match='Incomplete'):
        evaluation_receipt([row], seeds, True)
