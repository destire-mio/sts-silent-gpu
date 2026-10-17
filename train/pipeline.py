from dataclasses import dataclass
from datetime import timedelta
from multiprocessing.connection import wait
from types import SimpleNamespace
import time

import numpy as np
import torch
import torch.distributed as dist
import torch.multiprocessing as mp

from .checkpoint import save_checkpoint
from .distributed import distributed_policy, worker_device
from .model import Policy
from .ppo import update
from .rollout import Collector


@dataclass
class PipelineConfig:
    workers: int = 1
    batch_size: int = 32
    decision_cap: int = 4096
    storage_mib: int = 256
    device: str = "cuda"
    torch_threads: int = 1


def worker(connection, rank, model_config, ppo_config, config, seed, rendezvous, output, manifest):
    torch.set_num_threads(config.torch_threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(seed)
    device = worker_device(config.device, rank)
    model = Policy(model_config).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=ppo_config.learning_rate)
    collector = Collector(config.batch_size, config.decision_cap, config.storage_mib, device, ppo_config.gae_lambda)
    dist.init_process_group("nccl" if device.type == "cuda" else "gloo", init_method=rendezvous,
                            rank=rank, world_size=config.workers, timeout=timedelta(seconds=300))
    wrapped = distributed_policy(model, device)
    rng = torch.Generator(device=device).manual_seed(seed)
    version = 0
    if rank == 0:
        save_checkpoint(output / "initial.pt", model, optimizer, manifest, version)
        save_checkpoint(output / "checkpoint.pt", model, optimizer, manifest, version)
    connection.send(version)
    rollout = None
    while (job := connection.recv()) is not None:
        kind, expected, *payload = job
        assert expected == version
        if kind == "collect":
            assert rollout is None
            roots, sampling_seed, attempts, budget = payload
            rollout = collector.collect(model, roots, sampling_seed, attempts, budget, policy_version=version)
            connection.send(dict(version=version, metrics=rollout.metrics_host(), episodes=rollout.episodes,
                                 count=rollout.count, compact_bytes=int(rollout.offsets[rollout.count]), timing=rollout.timing))
        else:
            assert kind == "update" and rollout is not None
            metrics = update(wrapped, optimizer, rollout, ppo_config, device, rng, policy_version=version)
            rollout = None
            version += 1
            dist.barrier()
            if rank == 0:
                save_checkpoint(output / "checkpoint.pt", model, optimizer, manifest, version)
            connection.send((version, metrics))
    assert rollout is None
    dist.destroy_process_group()
    connection.close()


class Pipeline:
    def __init__(self, model_config, ppo_config, config, seed, output, manifest):
        assert config.workers > 0 and config.storage_mib > 0
        assert config.device in ("cpu", "cuda")
        assert config.device != "cpu" or config.workers <= 3
        self.model_config, self.ppo_config, self.config = model_config, ppo_config, config
        self.seed, self.output, self.manifest = seed, output, manifest
        self.context = mp.get_context("spawn")
        self.processes, self.connections = [], []
        self.version, self.pending = 0, None

    @property
    def state(self):
        return torch.load(self.output / "checkpoint.pt", map_location="cpu", weights_only=True)["model"]

    def receive(self):
        pending, results = set(self.connections), {}
        while pending:
            ready = wait([*pending, *(p.sentinel for p in self.processes)])
            for connection in pending.intersection(ready):
                results[connection] = connection.recv()
                pending.remove(connection)
            for process in self.processes:
                if process.sentinel in ready:
                    process.join()
                    raise RuntimeError(f"Training worker {process.pid} exited with {process.exitcode}")
        return [results[connection] for connection in self.connections]

    def __enter__(self):
        return self

    def initialize(self):
        for rank in range(self.config.workers):
            parent, child = self.context.Pipe()
            process = self.context.Process(target=worker, args=(child, rank, self.model_config, self.ppo_config,
                self.config, self.seed, (self.output / "rendezvous").resolve().as_uri(), self.output, self.manifest))
            process.start()
            child.close()
            self.processes.append(process)
            self.connections.append(parent)
        assert self.receive() == [0] * self.config.workers

    def collect(self, roots, sampling_seed, attempts, decision_budget=None):
        assert self.pending is None and len(roots) == len(attempts) and roots
        start = time.perf_counter()
        budget = len(roots) * self.config.decision_cap if decision_budget is None else decision_budget
        assert budget > 0
        count = min(self.config.workers, len(roots), budget)
        attempts = list(attempts)
        for rank, connection in enumerate(self.connections):
            group, ids = (roots[rank::count], attempts[rank::count]) if rank < count else ([], [])
            allowance = None if decision_budget is None else (budget // count + (rank < budget % count) if rank < count else 0)
            connection.send(("collect", self.version, group, sampling_seed, ids, allowance))
        parts = self.receive()
        assert all(p["version"] == self.version for p in parts)
        records, episodes = 0, []
        for part in parts:
            episodes.extend(dict(e, records=[i + records for i in e["records"]]) for e in part["episodes"])
            records += part["count"]
        metrics = {name: np.concatenate([p["metrics"][name] for p in parts]) for name in parts[0]["metrics"]}
        timing = {name: sum(p["timing"][name] for p in parts) for name in
                  ("engine_encode_seconds", "inference_seconds", "storage_seconds")}
        timing["collection_seconds"] = time.perf_counter() - start
        result = SimpleNamespace(**metrics, count=records, episodes=sorted(episodes, key=lambda e: e["attempt"]),
                                 timing=timing, compact_bytes=sum(p["compact_bytes"] for p in parts) + (records + len(parts)) * 8)
        self.pending = result
        return result

    def update(self, rollout):
        assert self.pending is rollout
        assert np.all(rollout.policy_versions == self.version), "Stale policy samples"
        start = time.perf_counter()
        for connection in self.connections:
            connection.send(("update", self.version))
        results = self.receive()
        assert all(version == self.version + 1 for version, _ in results)
        self.version, metrics = results[0]
        self.pending = None
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
