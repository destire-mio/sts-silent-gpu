from contextlib import nullcontext

import torch
from torch.nn.parallel import DistributedDataParallel
from torch.distributed.algorithms.ddp_comm_hooks.default_hooks import allreduce_hook


def worker_device(name, rank):
    device = torch.device(name, rank) if name == "cuda" else torch.device(name)
    if device.type == "cuda":
        torch.cuda.set_device(device)
    return device


def cpu_work(device, gate):
    return gate if device.type == "cpu" else nullcontext()


def release_cpu_and_reduce(gate, bucket):
    if bucket.is_last():
        gate.release()
    return allreduce_hook(None, bucket)


def distributed_policy(model, device, gate):
    wrapped = DistributedDataParallel(model, device_ids=[device.index] if device.type == "cuda" else None,
                                      forward_sync_buffers=False)
    if device.type == "cpu":
        def acquire_cpu(module, inputs):
            gate.acquire()
        model.register_forward_pre_hook(acquire_cpu)
        wrapped.register_comm_hook(gate, release_cpu_and_reduce)
    return wrapped
