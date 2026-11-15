"""Fit an offline value network to the frozen E4 policy's remaining returns."""

import argparse
from collections import Counter
from copy import deepcopy
from dataclasses import asdict
from hashlib import file_digest
from itertools import islice
import json
import math
from pathlib import Path
import random
import time

import torch
from torch import nn
from torch.nn import functional as F

from tools.e4_value_truth import collector, load_policy
from train.e4 import MAX_REWARD
from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig, length_batches, packed_inputs, record_lengths, store_inputs


DATA_FORMAT = "e4_critic_returns_v1"
CRITIC_FORMAT = "e4_search_critic_v1"
BINS = 101
SHARD_ROWS = 4096
SPLITS = ("train", "validation")


class SearchCritic(nn.Module):
    def __init__(self, policy, loss):
        super().__init__()
        self.policy = deepcopy(policy)
        self.policy.encoder_fp16 = False
        self.loss = loss
        if loss == "hlgauss":
            value = self.policy.value
            projection = nn.Linear(value[2].in_features, BINS, device=value[2].weight.device)
            self.policy.value = nn.Sequential(value[0], value[1], projection)
        edges = torch.linspace(0., 1., BINS + 1)
        self.register_buffer("edges", edges, persistent=False)
        self.register_buffer("centers", (edges[1:] + edges[:-1]) / 2, persistent=False)

    def outputs(self, inputs):
        _, pooled = self.policy.encode(inputs)
        return self.policy.value(pooled).float()

    def expected(self, outputs):
        if self.loss == "mse":
            return outputs.squeeze(-1)
        return (outputs.softmax(-1) * self.centers).sum(-1)

    def fit_loss(self, outputs, targets):
        if self.loss == "mse":
            return F.mse_loss(self.expected(outputs), targets)
        widths = (self.edges - targets[:, None]) / (math.sqrt(2) * (0.75 / BINS))
        mass = torch.special.erf(widths).diff(dim=-1)
        return F.cross_entropy(outputs, mass / mass.sum(dim=-1, keepdim=True))

    def forward(self, inputs):
        return self.expected(self.outputs(inputs))


def load_critic(path, device):
    package = torch.load(path, map_location="cpu", weights_only=True)
    if package["format"] != CRITIC_FORMAT or package["loss"] not in ("mse", "hlgauss"):
        raise ValueError("Expected an E4 search critic checkpoint")
    source = CLSPolicy(ModelConfig(**package["config"]))
    critic = SearchCritic(source, package["loss"])
    critic.load_state_dict(package["state_dict"])
    return critic.to(device).eval()


def seed_range(value):
    try:
        start, stop = map(int, value.split(":"))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Supply a stop-exclusive START:STOP seed range") from exc
    if not 1_000_000_000 <= start < stop <= 2_000_000_000:
        raise argparse.ArgumentTypeError("Use the E4 training seed range [1000000000, 2000000000)")
    return range(start, stop)


def positive(value):
    n = int(value)
    if n < 1:
        raise argparse.ArgumentTypeError("Use a positive integer")
    return n


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def write_shard(directory, split, rows, shard_names):
    name = f"{split}-{len(shard_names):05d}.pt"
    data = bytearray()
    offsets = [0]
    targets, seeds = [], []
    for record, target, seed in rows:
        data.extend(record)
        offsets.append(len(data))
        targets.append(target)
        seeds.append(seed)
    file = directory / name
    torch.save(dict(data=torch.frombuffer(data, dtype=torch.uint8),
                    offsets=torch.tensor(offsets), targets=torch.tensor(targets),
                    seeds=torch.tensor(seeds)), file)
    shard_names.append(name)
    return file.stat().st_size


@torch.no_grad()
def collect_returns(policy, seeds, directory, device, *, lanes, every, max_points, seed):
    directory.mkdir()
    start = time.monotonic()
    runner = collector(lanes, device, seed, float("inf"), greedy=True)
    search = runner.search
    data = torch.empty(lanes * 32768, dtype=torch.uint8, device=device)
    offsets = torch.zeros(lanes + 1, dtype=torch.long, device=device)
    episode_rows = [[] for _ in range(lanes)]
    pending = {split: [] for split in SPLITS}
    shards = {split: [] for split in SPLITS}
    points = dict.fromkeys(SPLITS, 0)
    outcomes = Counter()
    shard_bytes = 0
    waiting = iter(seeds)
    live = []
    while True:
        free = [i for i, active_seed in enumerate(runner.seeds) if active_seed is None]
        if sum(points.values()) < max_points:
            new_seeds = list(islice(waiting, len(free)))
            if new_seeds:
                runner.reset(free[:len(new_seeds)], new_seeds)
                live.extend(free[:len(new_seeds)])
        if not live:
            break
        before = search.progress_for(live)
        actions, _, _, _ = runner.choose(policy, live, before)
        sample = [i for i in live if i not in search.failed and len(runner.paths[i]) % every == 0]
        if sample:
            indices = torch.tensor(sample, dtype=torch.long, device=device)
            batch_offsets = offsets[:len(sample) + 1]
            store_inputs(search.batch, search.arrays, indices, data, batch_offsets)
            boundaries = batch_offsets.tolist()
            packed = data[:boundaries[-1]].cpu().numpy().tobytes()
            for slot, left, right in zip(sample, boundaries, boundaries[1:]):
                episode_rows[slot].append((packed[left:right], runner.rewards[slot].total))

        _, terminal = runner.advance(live, actions, before)
        finished = [slot for slot, done in zip(live, terminal) if done]
        for slot, episode in zip(finished, runner.completed, strict=True):
            outcome = episode["outcome"]
            outcomes[outcome] += 1
            if outcome != "stalled":
                game_seed = episode["seed"]
                split = "validation" if game_seed % 10 == 0 else "train"
                earned = runner.rewards[slot].total
                rows = episode_rows[slot]
                points[split] += len(rows)
                pending[split].extend((record, (earned - previous) / MAX_REWARD, game_seed)
                                      for record, previous in rows)
                if len(pending[split]) >= SHARD_ROWS:
                    shard_bytes += write_shard(directory, split, pending[split], shards[split])
                    pending[split].clear()
            episode_rows[slot].clear()
        runner.completed.clear()
        live = [slot for slot, done in zip(live, terminal) if not done]

    for split, rows in pending.items():
        if rows:
            shard_bytes += write_shard(directory, split, rows, shards[split])
    elapsed = time.monotonic() - start
    games = sum(outcomes.values())
    return dict(format=DATA_FORMAT, key_last_chance=False, policy_mode="argmax",
                device=str(device), lanes=lanes, every=every, max_points=max_points,
                seed_start=seeds.start, seed_stop_exclusive=seeds.stop, rng_seed=seed,
                reward_divisor=MAX_REWARD, episodes=games, outcomes=dict(outcomes),
                rows=points, shards=shards, shard_bytes=shard_bytes,
                elapsed_seconds=elapsed, seconds_per_1000_episodes=1000 * elapsed / games,
                shard_bytes_per_1000_episodes=1000 * shard_bytes / games)


def read_shard(path, device):
    return torch.load(path, map_location=device, weights_only=True)


def input_batch(shard, ids, device):
    return packed_inputs(shard["data"], shard["offsets"], ids, device)


@torch.no_grad()
def evaluate(critic, paths, device, batch_size):
    critic.eval()
    estimates, targets = [], []
    for path in paths:
        shard = read_shard(path, device)
        rows = torch.arange(len(shard["targets"]), device=device)
        for ids in rows.split(batch_size):
            estimates.append(critic(input_batch(shard, ids, device)).cpu())
            targets.append(shard["targets"][ids].cpu())
    estimate, target = torch.cat(estimates).double(), torch.cat(targets).double()
    mse = (estimate - target).square().mean().item()
    x, y = estimate - estimate.mean(), target - target.mean()
    scale = x.norm() * y.norm()
    return dict(validation_rows=len(target), mse=mse,
                pearson=(x.dot(y) / scale).clamp(-1, 1).item() if scale > 0 else None)


def training_batches(paths, device, batch_size, seed):
    shuffle = random.Random(seed)
    generator = torch.Generator(device=device).manual_seed(seed)
    while True:
        shuffle.shuffle(paths)
        for path in paths:
            shard = read_shard(path, device)
            rows = torch.arange(len(shard["targets"]), device=device)
            lengths = record_lengths(shard["data"], shard["offsets"], rows)
            for ids in length_batches(rows, *lengths, batch_size, generator):
                yield input_batch(shard, ids, device), shard["targets"][ids]


def fit_critic(policy, data_directory, output_directory, device, *, loss, steps, batch_size,
               eval_every, lr, seed, policy_sha256):
    manifest = json.loads((data_directory / "manifest.json").read_text(encoding="utf-8"))
    if manifest["format"] != DATA_FORMAT or manifest["policy_sha256"] != policy_sha256:
        raise ValueError("Use a returns dataset collected with this policy checkpoint")
    train, validation = ([data_directory / name for name in manifest["shards"][split]] for split in SPLITS)
    if not train or not validation:
        raise ValueError("Need completed games in both seed splits")
    output_directory.mkdir()
    torch.manual_seed(seed)
    critic = SearchCritic(policy, loss).to(device).train()
    optimizer = torch.optim.Adam(critic.parameters(), lr=lr)
    metrics = []
    batches = training_batches(train, device, batch_size, seed)
    for step, (inputs, target) in enumerate(islice(batches, steps), 1):
        optimizer.zero_grad(set_to_none=True)
        objective = critic.fit_loss(critic.outputs(inputs), target)
        objective.backward()
        optimizer.step()
        if step % eval_every == 0 or step == steps:
            result = dict(step=step, train_loss=objective.item(),
                          **evaluate(critic, validation, device, batch_size))
            metrics.append(result)
            print(json.dumps(result, allow_nan=False), flush=True)
            critic.train()

    torch.save(dict(format=CRITIC_FORMAT, loss=loss, config=asdict(policy.config),
                    policy_sha256=policy_sha256, state_dict=critic.state_dict()),
               output_directory / "critic.pt")
    write_json(output_directory / "fit.json",
               dict(loss=loss, steps=steps, learning_rate=lr, batch_size=batch_size,
                    policy_sha256=policy_sha256, rng_seed=seed, metrics=metrics))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    gather = commands.add_parser("collect", help="Play games and save packed Monte Carlo targets")
    fit = commands.add_parser("fit", help="Fit a standalone critic on the saved targets")
    for command in (gather, fit):
        command.add_argument("--policy", required=True, type=Path)
        command.add_argument("--output", required=True, type=Path, help="New output directory")
        command.add_argument("--device", default="cuda:0")
        command.add_argument("--seed", type=int, default=101)
    gather.add_argument("--seeds", required=True, type=seed_range)
    gather.add_argument("--every", type=positive, default=8)
    gather.add_argument("--lanes", type=positive, default=128)
    gather.add_argument("--max-points", type=positive, default=2_000_000)
    gather.add_argument("--pilot-episodes", type=positive, help="Collect at most this many seeds")
    gather.add_argument("--fp32", action="store_true")
    fit.add_argument("--data", type=Path, required=True)
    fit.add_argument("--loss", choices=("mse", "hlgauss"), default="mse")
    fit.add_argument("--steps", type=positive, default=20_000)
    fit.add_argument("--batch-size", type=positive, default=128)
    fit.add_argument("--eval-every", type=positive, default=5_000)
    fit.add_argument("--lr", type=float, default=3e-5)
    args = parser.parse_args()
    torch.set_num_threads(1)
    device = torch.device(args.device)
    if device.type == "cuda":
        torch.cuda.set_device(device.index if device.index is not None else 0)
    with args.policy.open("rb") as stream:
        policy_sha256 = file_digest(stream, "sha256").hexdigest()
    policy = load_policy(args.policy, device, fp32=args.command == "fit" or args.fp32)
    if args.command == "collect":
        seeds = args.seeds if args.pilot_episodes is None else args.seeds[:args.pilot_episodes]
        result = collect_returns(policy, seeds, args.output, device, lanes=args.lanes,
                                 every=args.every, max_points=args.max_points, seed=args.seed)
        result.update(policy_sha256=policy_sha256, policy_fp32=not policy.encoder_fp16,
                      pilot_episodes=args.pilot_episodes)
        write_json(args.output / "manifest.json", result)
        print(json.dumps(result, allow_nan=False), flush=True)
    else:
        fit_critic(policy, args.data, args.output, device, loss=args.loss, steps=args.steps,
                   batch_size=args.batch_size, eval_every=args.eval_every, lr=args.lr,
                   seed=args.seed, policy_sha256=policy_sha256)


if __name__ == "__main__":
    main()

