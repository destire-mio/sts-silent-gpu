import torch
from torch.nn.parallel import DistributedDataParallel


def worker_device(name, rank):
    device = torch.device(name, rank) if name == "cuda" else torch.device(name)
    if device.type == "cuda":
        torch.cuda.set_device(device)
    return device


def distributed_policy(model, device):
    return DistributedDataParallel(model, device_ids=[device.index] if device.type == "cuda" else None,
                                   forward_sync_buffers=False)
