import torch
from train.e4_lookahead import preview_faults_present


def test_uint32_preview_faults_preserve_sparse_selection_and_high_bits():
    flags = torch.tensor([0, 1, 0, 2**31, 2**32-1], dtype=torch.uint32)
    before = flags.clone()
    assert not preview_faults_present(flags, [0, 2])
    assert preview_faults_present(flags, [3])
    assert preview_faults_present(flags, [4, 0])
    assert preview_faults_present(flags, [1])
    assert not preview_faults_present(flags, [])
    assert torch.equal(flags, before)
