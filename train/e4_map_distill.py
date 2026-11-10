"""Opt-in map head and its independent optimizer for the standalone E4 fork."""
import base64
from collections import defaultdict
import gzip
import hashlib
import json
import math
import random
import statistics

import torch
from torch import nn

from train.e4 import KEYS, MAP
from train.e4_key_last_chance import EMERALD
from train.e4_midstart import MidstartCollector
from train.e4_stage12 import CLSPolicy
from train.model import packed_inputs

LABELS_PER_UPDATE = 256


class RouteHead(nn.Module):
    def __init__(self, width, device):
        super().__init__()
        self.weight = nn.Parameter(torch.zeros(1, width, device=device))
        self.bias = nn.Parameter(torch.zeros(1, device=device))

    def forward(self, entities, destination_refs):
        scores = nn.functional.linear(entities.detach(), self.weight, self.bias).squeeze(-1)
        scores = nn.functional.pad(scores, (1, 0))
        return scores.gather(1, destination_refs)


class MapDistillPolicy(CLSPolicy):
    def __init__(self, config, weight, device):
        super().__init__(config)
        self.map_weight = weight
        # Owned and checkpointed by MapDistillation. DDP, PPO and policy EMA see the
        # original parameter list. Raw and EMA evaluation both use the current head.
        object.__setattr__(self, "route_head", RouteHead(config.width, device))

    def action_logits(self, observation, entities, pooled):
        logits = super().action_logits(observation, entities, pooled)
        if self.map_weight == 0:
            return logits
        rows = torch.where(observation["ids"][:, 0, 0] == MAP + 1)[0]
        if not len(rows):
            return logits
        with torch.no_grad():
            residual = self.route_head(entities[rows], observation["candidate_refs"][rows, 2])
            routes = observation["candidate_kinds"][rows] == 3
            residual = residual.masked_fill(~routes, 0.)
        logits[rows] = logits[rows] + residual
        return logits


class MapDistillCollector(MidstartCollector):
    def advance(self, slots, actions, progress):
        previous = len(self.completed)
        rewards, terminal = super().advance(slots, actions, progress)
        ended = (slot for slot, done in zip(slots, terminal) if done)
        for episode, slot in zip(self.completed[previous:], ended):
            if episode["outcome"] != "stalled":
                episode["map_distill_emerald"] = bool(self.search.last_step_progress[slot][KEYS] & EMERALD)
        return rewards, terminal


def training_reach(episodes):
    valid = [e for e in episodes if e["outcome"] != "stalled"]
    n = len(valid)
    act4 = sum(e["reach"]["act4"] for e in valid)
    emerald = sum(e["map_distill_emerald"] for e in valid)
    return dict(completed=n, act4=act4, act4_rate=act4 / n if n else None,
                emerald=emerald, emerald_rate=emerald / n if n else None)


def target_rows(rows):
    """Pool variance of matched option-minus-argmax draws by act."""
    groups = defaultdict(list)
    for row in rows:
        groups[row["act"]].extend(o for o in row["estimates"] if o["action"] != row["argmax"])
    variances = {}
    for act, options in groups.items():
        degrees = sum(o["k"] - 1 for o in options)
        paired_var = sum((o["k"] - 1) * o["paired_se"]**2 * o["k"] for o in options) / degrees
        total = statistics.variance(o["delta"] for o in options) if len(options) > 1 else 0.
        noise = statistics.mean(paired_var / o["k"] for o in options)
        variances[act] = (paired_var, max(0., total - noise))

    def adjusted(delta, k, paired_var, signal):
        se_sq = paired_var / k
        if signal == 0 or delta == 0:
            return 0.
        if se_sq == 0:
            return math.copysign(3., delta)
        z = delta * signal / (signal + se_sq) / math.sqrt(se_sq)
        return min(3., max(-3., z))

    targets = []
    for row in rows:
        paired_var, signal = variances[row["act"]]
        options = row["estimates"]
        logits = [ref + adjusted(o["delta"], o["k"], paired_var, signal)
                  for ref, o in zip(row["p_ref_logits"], options)]
        targets.append((base64.b64decode(row["packed_observation"], validate=True), row["options"], logits))
    return targets, variances


def load_targets(directory):
    manifest_bytes = (directory / "manifest.json").read_bytes()
    manifest = json.loads(manifest_bytes)
    if manifest.get("format") != 1 or manifest.get("mode") != "labels":
        raise ValueError("Expected E4 map label format 1")
    fingerprint = hashlib.sha256(manifest_bytes)
    rows = []
    for start in range(0, manifest["roots"], manifest["shard_size"]):
        name = directory / f"labels-{start:07d}.json.gz"
        raw = name.read_bytes()
        shard = json.loads(gzip.decompress(raw))
        count = min(manifest["shard_size"], manifest["roots"] - start)
        if shard.get("format") != 1 or shard.get("mode") != "labels" or len(shard["rows"]) != count:
            raise ValueError(f"Unexpected label shard {name}")
        fingerprint.update(raw)
        for row in shard["rows"]:
            if not row["complete"]:
                continue
            if (len(row["options"]) < 2 or len(row["p_ref_logits"]) != len(row["options"])
                    or [o["action"] for o in row["estimates"]] != row["options"]
                    or not all(o["k"] in (4, 12) and o["paired_se"] is not None
                               and math.isfinite(o["paired_se"]) and o["paired_se"] >= 0
                               for o in row["estimates"])):
                raise ValueError(f"Invalid options in {name}")
            rows.append(row)
    if not rows:
        raise ValueError("The map label set contains no complete roots")
    targets, variances = target_rows(rows)
    return targets, variances, fingerprint.hexdigest()


def batch_targets(rows, device):
    data = bytearray()
    offsets = []
    for packed, _, _ in rows:
        offsets.append(len(data))
        data.extend(packed)
    inputs = packed_inputs(torch.frombuffer(data, dtype=torch.uint8).to(device),
                           torch.tensor(offsets, device=device), torch.arange(len(rows), device=device), device)
    teacher = torch.full(inputs["candidate_mask"].shape, -torch.inf, device=device)
    batch, actions, values = [], [], []
    for i, (_, options, logits) in enumerate(rows):
        batch.extend([i] * len(options))
        actions.extend(options)
        values.extend(logits)
    if not bool((inputs["candidate_mask"][batch, actions] & (inputs["candidate_kinds"][batch, actions] == 3)
                 & (inputs["candidate_refs"][batch, 2, actions] > 0)).all()):
        raise ValueError("Map labels do not match the packed legal route actions")
    teacher[batch, actions] = torch.tensor(values, device=device)
    inputs["candidate_mask"] = teacher.isfinite()
    return inputs, teacher


def train_head_step(policy, optimizer, inputs, teacher_logits):
    with torch.no_grad():
        entities, pooled = policy.encode(inputs)
        base = CLSPolicy.action_logits(policy, inputs, entities, pooled).float()
    residual = policy.route_head(entities, inputs["candidate_refs"][:, 2]).float()
    legal = inputs["candidate_mask"]
    log_target = teacher_logits.log_softmax(-1)
    log_student = (base + residual).log_softmax(-1)
    kl = (log_target.exp() * (log_target.masked_fill(~legal, 0.)
                              - log_student.masked_fill(~legal, 0.))).sum(-1).mean()
    if not bool(kl.isfinite()):
        raise FloatingPointError("Nonfinite map distillation loss")
    optimizer.zero_grad(set_to_none=True)
    (policy.map_weight * kl).backward()
    optimizer.step()
    with torch.no_grad():
        magnitude = policy.route_head(entities, inputs["candidate_refs"][:, 2])[legal].abs().mean()
    return dict(loss=float(kl.detach()), head_mean_abs=float(magnitude), label_roots=len(base))


class MapDistillation:
    def __init__(self, policy, directory, saved):
        self.policy = policy
        self.targets, self.variances, fingerprint = load_targets(directory)
        self.identity = dict(labels_sha256=fingerprint, weight=policy.map_weight)
        self.optimizer = torch.optim.Adam(policy.route_head.parameters(), lr=3e-4)
        if saved is not None:
            if saved["identity"] != self.identity:
                raise ValueError("Map distillation resume requires the same labels and weight")
            policy.route_head.load_state_dict(saved["head"])
            self.optimizer.load_state_dict(saved["optimizer"])

    def state_dict(self):
        return dict(identity=self.identity, head=self.policy.route_head.state_dict(),
                    optimizer=self.optimizer.state_dict())

    def step(self, update, device):
        if self.policy.map_weight == 0:
            return dict(loss=0., head_mean_abs=0., label_roots=0)
        rng = random.Random(20261110 + update)
        if len(self.targets) >= LABELS_PER_UPDATE:
            rows = rng.sample(self.targets, LABELS_PER_UPDATE)
        else:
            rows = rng.choices(self.targets, k=LABELS_PER_UPDATE)
        inputs, teacher = batch_targets(rows, device)
        return train_head_step(self.policy, self.optimizer, inputs, teacher)
