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
    with torch.cuda.stream(stream):
        cpu_args = [torch.tensor(x, dtype=t) for x, t in (([32, 0], torch.int64), ([1, 0], torch.int64),
            ([123, -1], torch.int64), ([0, 1, 3], torch.int64), ([0, 0, 0], torch.int32))]
        gpu_args = [x.cuda() for x in cpu_args]
        gpu.reset_tensors(*gpu_args)
        cpu.reset_tensors(*cpu_args)
        mask = torch.zeros(33, dtype=torch.uint8); mask[[0, 32]] = 1
        choices = torch.zeros(33, dtype=torch.int32)
        gpu.step_tensors(choices.cuda(), mask.cuda())
        cpu.step_tensors(choices, mask)
        slots = torch.tensor([32, 0], dtype=torch.int64)
        offsets = torch.zeros(3, dtype=torch.int64)
        lengths = 4 + 24 * cpu.entity_counts[[32, 0]] + 7 * cpu.candidate_counts[[32, 0]]
        offsets[1:] = torch.as_tensor(lengths).cumsum(0)
        host_data = torch.empty(int(offsets[-1]), dtype=torch.uint8)
        device_data = torch.empty_like(host_data, device='cuda')
        cpu.store_tensors(slots, host_data, offsets)
        gpu.store_tensors(slots.cuda(), device_data, offsets.cuda())
    stream.synchronize()
    torch.testing.assert_close(host_data, device_data.cpu(), atol=0, rtol=0)
    for field, value in arrays.items():
        assert value.cpu().numpy().tobytes() == getattr(cpu, field).tobytes(), field
    del gpu
    gc.collect()
    assert arrays['tags'].cpu().numpy().tobytes() == cpu.tags.tobytes()
    print('CUDA Python: zero-copy tensors, non-default stream, lifecycle, roots and owner lifetime PASS')


if __name__ == '__main__':
    check()
