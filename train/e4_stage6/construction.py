import random

import numpy as np
import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train.e4 import AllocationEnded, RoomSearch
from train.e4_stage6.copy_search import CopySearch, TrainingCollector, CopyCollector
from train.e4_stage6.initialization import hot_policy
from train.e4_stage6.training_worker import TrainingWorker


def bind_parent_stop(search, stop_file):
    clock = search.clock
    def checked_clock():
        clock()
        if stop_file.exists():
            raise AllocationEnded()
    search.clock = checked_clock


def create_training_worker(source, source_sha256, runtime_commit, arm, device, evidence_directory, stop_file):
    rank, world = dist.get_rank(), dist.get_world_size()
    assert 0 <= rank < 8 and world <= 8
    policy, optimizer = hot_policy(source, arm, device)
    model = DistributedDataParallel(policy, device_ids=[device.index] if device.type == 'cuda' else None,
        broadcast_buffers=False)
    seed_rng = torch.Generator()
    seed_rng.set_state(source['state']['train_state']['seed_rng'])
    search_rng = torch.Generator(device=device)
    search_rng.set_state(source['state']['train_state']['search_rng'])
    if arm == 'A':
        search = RoomSearch(256, 32, device, search_rng, scratch_slots=256*33, bosses_seen=511)
        collector = TrainingCollector(search, seed_rng, rank=rank)
    else:
        selection_rng = torch.Generator().manual_seed(1729+rank)
        search = CopySearch(256, device, search_rng, selection_rng, bosses_seen=511)
        collector = CopyCollector(search, seed_rng, rank=rank)
    search.evidence_directory = evidence_directory
    bind_parent_stop(search, stop_file)
    ppo_rng = torch.Generator(device=device)
    ppo_rng.set_state(source['ppo_rng'])
    binding = dict(arm=arm, rank=rank, world=world, bosses_seen=511, lanes=256, trials=32,
        epochs=4, initialization=1729, source_rank=rank, source_world=8, source_sha256=source_sha256,
        source_commit=source['commit'], source_update=8, source_Adam_step=512,
        runtime_commit=runtime_commit, config=source['config'].copy())
    worker = TrainingWorker(model, optimizer, collector, ppo_rng, binding,
        source['state']['imitation_beta'], source['state']['imitation_entropy_target'])
    torch.set_rng_state(source['state']['training_rng']['torch'])
    random.setstate(source['state']['training_rng']['python'])
    np.random.seed(1729+rank)
    if device.type == 'cuda':
        torch.cuda.set_rng_state(source['state']['training_rng']['cuda'], device)
    return worker
