from copy import deepcopy
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
from .ppo import empty_inputs, update
from .rollout import Collector, DEFAULT_INFERENCE_CHUNK


@dataclass
class PipelineConfig:
    workers: int = 1
    batch_size: int = 32
    decision_cap: int = 4096
    storage_mib: int = 256
    device: str = "cuda"
    torch_threads: int = 1
    inference_chunk: int = DEFAULT_INFERENCE_CHUNK


def device_memory(device):
    if device.type == "cpu":
        return {}
    free, total = torch.cuda.mem_get_info(device)
    return dict(allocated_bytes=torch.cuda.memory_allocated(device), reserved_bytes=torch.cuda.memory_reserved(device),
                peak_allocated_bytes=torch.cuda.max_memory_allocated(device),
                peak_reserved_bytes=torch.cuda.max_memory_reserved(device), free_bytes=free, total_bytes=total)


def worker(connection, rank, model_config, ppo_config, config, seed, rendezvous, output, manifest, checkpoint):
    torch.set_num_threads(config.torch_threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(seed)
    device = worker_device(config.device, rank)
    model = Policy(model_config).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=ppo_config.learning_rate)
    if checkpoint is not None:
        model.load_state_dict(checkpoint["model"])
        optimizer.load_state_dict(deepcopy(checkpoint["optimizer"]))
    collector = Collector(config.batch_size, config.decision_cap, config.storage_mib, device, ppo_config.gae_lambda,
                          inference_chunk=config.inference_chunk)
    dist.init_process_group("nccl" if device.type == "cuda" else "gloo", init_method=rendezvous,
                            rank=rank, world_size=config.workers, timeout=timedelta(seconds=300))
    wrapped = distributed_policy(model, device)
    # DDP rebuilds its reduction buckets after the first backward. Stabilize
    # their order before real gradients so fresh and resumed workers agree.
    for _ in range(2):
        logits, value = wrapped(empty_inputs(device))
        ((logits.sum() + value.sum()) * 0).backward()
        optimizer.zero_grad(set_to_none=True)
    rng = torch.Generator(device=device).manual_seed(seed)
    version, next_attempt = 0, 0
    if checkpoint is not None:
        version, next_attempt = checkpoint["policy_version"], checkpoint["next_attempt"]
        state = checkpoint["rng_states"][rank]
        torch.set_rng_state(state["torch"])
        rng.set_state(state["ppo"])
        if device.type == "cuda":
            torch.cuda.set_rng_state(state["cuda"], device)
    save_checkpoint(output / "initial.pt", model, optimizer, manifest, version, rng, next_attempt)
    save_checkpoint(output / "checkpoint.pt", model, optimizer, manifest, version, rng, next_attempt)
    connection.send(version)
    rollout = None
    while (job := connection.recv()) is not None:
        kind, expected, *payload = job
        assert expected == version
        if kind == "collect":
            assert rollout is None
            if device.type == "cuda":
                torch.cuda.reset_peak_memory_stats(device)
            roots, sampling_seed, attempts, budget = payload
            rollout = collector.collect(model, roots, sampling_seed, attempts, budget, policy_version=version)
            connection.send(dict(version=version, metrics=rollout.metrics_host(), episodes=rollout.episodes,
                                 count=rollout.count, compact_bytes=int(rollout.offsets[rollout.count]), timing=rollout.timing,
                                 utilization=rollout.utilization, memory=device_memory(device),
                                 root_cache_bytes=sum(cache.bytes for cache in collector.root_caches)))
        else:
            assert kind == "update" and rollout is not None
            next_attempt, = payload
            metrics = update(wrapped, optimizer, rollout, ppo_config, device, rng, policy_version=version)
            rollout = None
            version += 1
            dist.barrier()
            save_checkpoint(output / "checkpoint.pt", model, optimizer, manifest, version, rng, next_attempt)
            metrics["memory"] = device_memory(device)
            connection.send((version, metrics))
    assert rollout is None
    dist.destroy_process_group()
    connection.close()


class Pipeline:
    def __init__(self, model_config, ppo_config, config, seed, output, manifest, checkpoint=None):
        assert config.workers > 0 and config.storage_mib > 0
        assert config.device in ("cpu", "cuda")
        assert config.device != "cpu" or config.workers <= 3
        self.model_config, self.ppo_config, self.config = model_config, ppo_config, config
        self.seed, self.output, self.manifest = seed, output, manifest
        self.context = mp.get_context("spawn")
        self.processes, self.connections = [], []
        self.version, self.pending = 0, None
        self.next_attempt, self.checkpoint = 0, checkpoint
        if checkpoint is not None:
            assert checkpoint["format_version"] == 1 and len(checkpoint["rng_states"]) == config.workers
            self.version, self.next_attempt = checkpoint["policy_version"], checkpoint["next_attempt"]

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
                self.config, self.seed, (self.output / "rendezvous").resolve().as_uri(), self.output, self.manifest,
                self.checkpoint))
            process.start()
            child.close()
            self.processes.append(process)
            self.connections.append(parent)
        assert self.receive() == [self.version] * self.config.workers

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
                  ("root_prepare_seconds", "reset_seconds", "step_encode_seconds", "inference_seconds", "storage_seconds")}
        timing["collection_seconds"] = time.perf_counter() - start
        utilization = {name: sum(p["utilization"][name] for p in parts) for name in parts[0]["utilization"]}
        utilization["step_fraction"] = (utilization["active_lane_steps"] / utilization["available_lane_steps"]
                                        if utilization["available_lane_steps"] else 0.)
        utilization["time_fraction"] = (utilization["active_lane_seconds"] / utilization["available_lane_seconds"]
                                        if utilization["available_lane_seconds"] else 0.)
        result = SimpleNamespace(**metrics, count=records, next_attempt=max(attempts) + 1,
                                 episodes=sorted(episodes, key=lambda e: e["attempt"]),
                                 utilization=utilization, workers=[dict(rank=rank, decisions=p["count"],
                                     compact_bytes=p["compact_bytes"], timing=p["timing"], utilization=p["utilization"],
                                     memory=p["memory"], root_cache_bytes=p["root_cache_bytes"])
                                     for rank, p in enumerate(parts)],
                                 timing=timing, compact_bytes=sum(p["compact_bytes"] for p in parts) + (records + len(parts)) * 8)
        self.pending = result
        return result

    def update(self, rollout):
        assert self.pending is rollout
        assert np.all(rollout.policy_versions == self.version), "Stale policy samples"
        start = time.perf_counter()
        for connection in self.connections:
            connection.send(("update", self.version, rollout.next_attempt))
        results = self.receive()
        assert all(version == self.version + 1 for version, _ in results)
        self.version, metrics = results[0]
        self.next_attempt = rollout.next_attempt
        self.pending = None
        memory = [dict(rank=rank, **m.pop("memory")) for rank, (_, m) in enumerate(results)]
        return dict(metrics, memory=memory, policy_version=self.version - 1, published_version=self.version,
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
