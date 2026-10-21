import json

import torch
import spire_gpu


def check():
    batch = spire_gpu.TrainingBatch(3, 2**63 - 1, [])
    batch.reset_to(0, 2000000544, [])
    batch.clone(0, 1)
    faults = torch.as_tensor(batch.faults, device='cuda:0')
    faults[1] = spire_gpu.PLAYED_CARDS_CAPACITY_FAULT
    batch.clone(1, 2)
    assert int(faults[2]) == spire_gpu.PLAYED_CARDS_CAPACITY_FAULT
    batch.clone(0, 2)
    assert int(faults[2]) == 0
    assert batch.state_bytes(2) == batch.state_bytes(0)
    print(json.dumps(dict(clone_copies_fault=True, healthy_source_clears_reused_slot=True,
                          full_state_equal=True)))


if __name__ == '__main__':
    check()
