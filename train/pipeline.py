from dataclasses import dataclass, fields
from datetime import timedelta
from multiprocessing.connection import wait
import time

import numpy as np
import torch
import torch.distributed as dist
import torch.multiprocessing as mp

from .checkpoint import save_checkpoint
from .distributed import cpu_work, distributed_policy, worker_device
from .model import Policy
from .ppo import update
from .rollout import Collector, Rollout


@dataclass
class PipelineConfig:
    actors: int = 2
    learners: int = 1
    batch_size: int = 32
    decision_cap: int = 4096
    storage_mib: int = 256
    device: str = "cpu"
    backend: str = "gloo"
    torch_threads: int = 4


def shared_rollout(rollout):
    result = {}
    for field in fields(rollout):
        value = getattr(rollout, field.name)
        if isinstance(value, np.ndarray):
            length = int(rollout.offsets[rollout.count]) if field.name == "data" else rollout.count + (field.name == "offsets")
            value = torch.from_numpy(value[:length].copy()).share_memory_()
        result[field.name] = value
    return result


def receive_rollout(payload):
    return Rollout(**{name: value.numpy() if isinstance(value, torch.Tensor) else value
                      for name, value in payload.items()})


def merge_rollouts(parts, elapsed):
    result, episodes, offset_parts = {}, [], [np.zeros(1, dtype=np.uint64)]
    records = byte_count = 0
    for part in parts:
        offset_parts.append(part.offsets[1:part.count + 1] + byte_count)
        for episode in part.episodes:
            episodes.append(dict(episode, records=[i + records for i in episode["records"]]))
        records += part.count
        byte_count += int(part.offsets[part.count])
    for field in fields(Rollout):
        if field.name in ("data", "offsets", "episodes", "timing", "count"):
            continue
        result[field.name] = np.concatenate([getattr(p, field.name)[:p.count] for p in parts])
    result["data"] = np.concatenate([p.data[:int(p.offsets[p.count])] for p in parts])
    result["offsets"] = np.concatenate(offset_parts)
    result["episodes"] = sorted(episodes, key=lambda e: e["attempt"])
    result["count"] = records
    result["timing"] = dict(collection_seconds=elapsed,
                            actor_seconds=[p.timing["collection_seconds"] for p in parts],
                            **{name: sum(p.timing[name] for p in parts) for name in
                               ("engine_encode_seconds", "inference_seconds", "storage_seconds")},
                            worker_seconds=np.sum([p.timing["worker_seconds"] for p in parts], axis=0).tolist())
    return Rollout(**result)


def policy_copy(model):
    return {name: value.detach().to(device="cpu", copy=True).share_memory_()
            for name, value in model.state_dict().items()}


def actor_worker(connection, actor, model_config, ppo_config, config, gate):
    torch.set_num_threads(config.torch_threads)
    torch.set_num_interop_threads(1)
    device = worker_device(config.device, actor % config.learners)
    with cpu_work(device, gate):
        model = Policy(model_config).to(device)
        collector = Collector(config.batch_size, config.decision_cap, config.storage_mib // config.actors,
                              device, ppo_config.gae_lambda)
    connection.send(dict(ready=True))
    last_version = -1
    while (job := connection.recv()) is not None:
        version, state, roots, seed, attempts, budget = job
        assert version > last_version
        with cpu_work(device, gate):
            model.load_state_dict(state)
            rollout = collector.collect(model, roots, seed, attempts, budget, policy_version=version)
        last_version = version
        payload = shared_rollout(rollout)
        connection.send((version, payload))
    connection.close()


def learner_worker(connection, rank, model_config, ppo_config, config, seed, rendezvous, output, manifest, gate):
    torch.set_num_threads(config.torch_threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(seed)
    device = worker_device(config.device, rank)
    with cpu_work(device, gate):
        model = Policy(model_config).to(device)
        optimizer = torch.optim.Adam(model.parameters(), lr=ppo_config.learning_rate)
    dist.init_process_group(config.backend, init_method=rendezvous, rank=rank, world_size=config.learners,
                            timeout=timedelta(seconds=120))
    wrapped = distributed_policy(model, device, gate)
    version = 0
    rng = np.random.default_rng(seed)
    if rank == 0:
        save_checkpoint(output / "initial.pt", model, optimizer, manifest, version)
        save_checkpoint(output / "checkpoint.pt", model, optimizer, manifest, version)
    state = policy_copy(model) if rank == 0 else None
    connection.send((version, state))
    while (job := connection.recv()) is not None:
        expected, payload = job
        assert expected == version
        rollout = receive_rollout(payload)
        metrics = update(wrapped, optimizer, rollout, ppo_config, device, rng, policy_version=version,
                         optimizer_context=cpu_work(device, gate))
        version += 1
        if rank == 0:
            save_checkpoint(output / "checkpoint.pt", model, optimizer, manifest, version)
        state = policy_copy(model) if rank == 0 else None
        connection.send((version, metrics, state))
    dist.destroy_process_group()
    connection.close()


class Pipeline:
    def __init__(self, model_config, ppo_config, config, seed, output, manifest):
        assert config.actors > 0 and config.learners > 0 and config.storage_mib >= config.actors
        assert config.device in ("cpu", "cuda")
        assert (config.device, config.backend) in (("cpu", "gloo"), ("cuda", "nccl"))
        self.model_config, self.ppo_config, self.config = model_config, ppo_config, config
        self.seed, self.output, self.manifest = seed, output, manifest
        self.context = mp.get_context("spawn")
        self.gate = self.context.Semaphore(1)
        self.processes, self.connections = [], []
        self.version, self.state = 0, None
        self.pending = False

    def start(self, target, *args):
        parent, child = self.context.Pipe()
        process = self.context.Process(target=target, args=(child, *args))
        process.start()
        child.close()
        self.processes.append(process)
        self.connections.append(parent)
        return parent

    def receive(self, connections):
        pending = set(connections)
        results = {}
        while pending:
            ready = wait([*pending, *(p.sentinel for p in self.processes)])
            for connection in pending.intersection(ready):
                results[connection] = connection.recv()
                pending.remove(connection)
            for process in self.processes:
                if process.sentinel in ready:
                    process.join()
                    raise RuntimeError(f"Training worker {process.pid} exited with {process.exitcode}")
        return [results[connection] for connection in connections]

    def __enter__(self):
        return self

    def initialize(self):
        self.learners = [self.start(learner_worker, rank, self.model_config, self.ppo_config, self.config,
                                    self.seed, (self.output / "rendezvous").resolve().as_uri(), self.output,
                                    self.manifest, self.gate) for rank in range(self.config.learners)]
        ready = self.receive(self.learners)
        assert all(version == 0 for version, _ in ready)
        self.state = ready[0][1]
        self.actors = [self.start(actor_worker, actor, self.model_config, self.ppo_config, self.config, self.gate)
                       for actor in range(self.config.actors)]
        self.receive(self.actors)

    def collect(self, roots, sampling_seed, attempts, decision_budget=None):
        assert not self.pending and len(roots) == len(attempts) and roots
        start = time.perf_counter()
        budget = len(roots) * self.config.decision_cap if decision_budget is None else decision_budget
        count = min(len(self.actors), len(roots), budget)
        active = self.actors[:count]
        attempts = list(attempts)
        for i, connection in enumerate(active):
            allowance = None if decision_budget is None else budget // count + (i < budget % count)
            connection.send((self.version, self.state, roots[i::count], sampling_seed, attempts[i::count], allowance))
        results = self.receive(active)
        assert all(version == self.version for version, _ in results)
        parts = [receive_rollout(payload) for _, payload in results]
        rollout = merge_rollouts(parts, time.perf_counter() - start)
        rollout.timing["collection_seconds"] = time.perf_counter() - start
        self.pending = True
        return rollout

    def update(self, rollout):
        assert self.pending
        assert np.all(rollout.policy_versions[:rollout.count] == self.version), "Stale policy samples"
        start = time.perf_counter()
        payload = shared_rollout(rollout)
        for connection in self.learners:
            connection.send((self.version, payload))
        results = self.receive(self.learners)
        assert all(version == self.version + 1 for version, _, _ in results)
        self.version, metrics, self.state = results[0]
        self.pending = False
        return dict(metrics, policy_version=self.version - 1, published_version=self.version,
                    learner_seconds=time.perf_counter() - start)

    def __exit__(self, kind, value, traceback):
        if kind is None:
            for connection in self.connections:
                connection.send(None)
        else:
            for process in self.processes:
                process.terminate()
        for process in self.processes:
            process.join()
        for connection in self.connections:
            connection.close()
