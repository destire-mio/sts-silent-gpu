"""Exercise the CUDA Python interface and its tensor lifetime/stream contract."""
import gc
import numpy as np
import torch
import spire
import spire_gpu

FIELDS = ('tags', 'ids', 'values', 'candidate_kinds', 'candidate_refs', 'entity_mask', 'candidate_mask',
          'entity_counts', 'candidate_counts', 'done', 'truncated', 'reset_mask', 'advanced', 'rewards',
          'faults', 'seed_indices', 'decision_counts')


def check():
    cpu = spire.TrainingBatch(33, 1, 11, list(range(2048)))
    gpu = spire_gpu.TrainingBatch(33, 11, list(range(2048)))
    arrays = {field: torch.as_tensor(getattr(gpu, field), device='cuda') for field in FIELDS}
    pointers = {field: value.data_ptr() for field, value in arrays.items()}
    stream = torch.cuda.Stream()
    rng = np.random.default_rng(1729)
    for _ in range(128):
        for field, value in arrays.items():
            assert value.is_cuda and value.data_ptr() == pointers[field]
            assert value.cpu().numpy().tobytes() == getattr(cpu, field).tobytes(), field
        selected = rng.integers(np.maximum(cpu.candidate_counts, 1), dtype=np.int32)
        with torch.cuda.stream(stream):
            actions = torch.as_tensor(selected, device='cuda')
            gpu.step(actions)
        cpu.step(selected)
    gpu.reset_to_many([0, 32], [123, -1], [[0], [0]])
    cpu.reset_to_many([0, 32], [123, -1], [[0], [0]])
    gpu.step_slots([32], [0]); cpu.step_slots([32], [0])
    for field, value in arrays.items():
        assert value.cpu().numpy().tobytes() == getattr(cpu, field).tobytes(), field
    del gpu
    gc.collect()
    assert arrays['tags'].cpu().numpy().tobytes() == cpu.tags.tobytes()
    print('CUDA Python: zero-copy tensors, non-default stream, lifecycle, roots and owner lifetime PASS')


if __name__ == '__main__':
    check()
