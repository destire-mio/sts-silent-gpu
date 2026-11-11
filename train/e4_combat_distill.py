"""Opt-in offline combat KL applied directly to the standalone policy trunk."""
import base64
import gzip
import hashlib
import json
import math
import random

import torch

from train.e4_map_distill import target_rows
from train.model import packed_inputs

LABELS_PER_UPDATE = 256


def load_combat_targets(directory):
    raw = (directory / "manifest.json").read_bytes()
    manifest = json.loads(raw)
    if manifest.get("format") != "e4_combat_labels_v1":
        raise ValueError("Expected combat label manifest")
    fingerprint = hashlib.sha256(raw)
    rows = []
    for file in sorted(directory.glob("labels-*.json.gz")):
        raw = file.read_bytes()
        fingerprint.update(raw)
        shard = json.loads(gzip.decompress(raw))
        if shard.get("format") != "e4_combat_labels_v1":
            raise ValueError(f"Unexpected combat shard {file}")
        for row in shard["rows"]:
            if not row["complete"]:
                continue
            options = row["options"]
            if (not 2 <= len(options) <= 4 or len(set(options)) != len(options)
                    or len(row["p_ref_logits"]) != len(options)
                    or len(row.get("p_ref_all_options", [])) != len(row.get("p_ref_all_logits", []))
                    or not set(options) <= set(row.get("p_ref_all_options", []))
                    or [item["action"] for item in row["estimates"]] != options
                    or row["argmax"] not in options
                    or not 1_000_000_000 <= row["seed"] < 2_000_000_000
                    or not all(item["k"] in (8, 24) and item["paired_se"] is not None
                               and math.isfinite(item["paired_se"]) and item["paired_se"] >= 0
                               and math.isfinite(item["delta"]) for item in row["estimates"])):
                raise ValueError(f"Invalid combat labels in {file}")
            rows.append(row)
    if not rows:
        raise ValueError("No complete combat labels")
    top_targets, variances = target_rows(rows)
    z = [target - ref for (_, _, logits), row in zip(top_targets, rows)
         for target, ref in zip(logits, row["p_ref_logits"])]
    targets = []
    for (packed, options, adjusted), row in zip(top_targets, rows):
        full_options = row["p_ref_all_options"]
        by_action = dict(zip(options, adjusted))
        logits = [by_action.get(action, ref) for action, ref in zip(full_options, row["p_ref_all_logits"])]
        if len(set(full_options)) != len(full_options) or not all(math.isfinite(value) for value in logits):
            raise ValueError("Invalid full reference policy support")
        targets.append((packed, full_options, logits))
    summary = dict(roots=len(rows), z_abs_gt_1_fraction=sum(abs(value) > 1 for value in z) / len(z),
                   z_min=min(z), z_max=max(z), paired_variances_by_act=variances)
    return targets, fingerprint.hexdigest(), summary


def combat_batch_targets(rows, device):
    data = bytearray()
    offsets = []
    for packed, _, _ in rows:
        offsets.append(len(data))
        data.extend(packed)
    inputs = packed_inputs(torch.frombuffer(data, dtype=torch.uint8).to(device),
                           torch.tensor(offsets, device=device), torch.arange(len(rows), device=device), device)
    teacher = torch.full(inputs["candidate_mask"].shape, -torch.inf, device=device)
    for i, (_, options, logits) in enumerate(rows):
        if not bool(inputs["candidate_mask"][i, options].all()):
            raise ValueError("Combat labels disagree with packed native legal actions")
        teacher[i, options] = torch.tensor(logits, device=device)
    # Unsearched legal actions retain z=0. With no evidence, the full reference
    # distribution is preserved rather than renormalized to the top-k subset.
    return inputs, teacher


def direct_policy_kl(logits, teacher_logits, legal):
    target_log = teacher_logits.log_softmax(-1)
    support = teacher_logits.isfinite()
    student_log = logits.float().masked_fill(~legal, -torch.inf).log_softmax(-1)
    return (target_log.exp() * (target_log.masked_fill(~support, 0.)
                               - student_log.masked_fill(~support, 0.))).sum(-1).mean()


def add_capped_combat_gradients(model, gradients, scaler, maximum_ratio):
    parameters = list(model.parameters())
    ppo = torch.linalg.vector_norm(torch.stack([
        parameter.grad.norm() if parameter.grad is not None else parameter.new_zeros(())
        for parameter in parameters]))
    distill = gradients.norm()
    factor = torch.minimum(distill.new_ones(()), maximum_ratio * ppo / distill.clamp_min(1e-30))
    offset = 0
    for parameter in parameters:
        end = offset + parameter.numel()
        contribution = gradients[offset:end].view_as(parameter) * factor
        if parameter.grad is None:
            parameter.grad = contribution.clone()
        else:
            parameter.grad.add_(contribution)
        offset = end
    denominator = float(ppo)
    return dict(ppo_gradient_norm=denominator / scaler.get_scale(),
                distill_gradient_norm=float(distill) / scaler.get_scale(),
                gradient_ratio_before_cap=float(distill) / denominator if denominator else None,
                gradient_ratio_after_cap=float(distill * factor) / denominator if denominator else 0.,
                gradient_scale=float(factor))


class CombatDistillation:
    def __init__(self, directory, weight, maximum_ratio, saved=None):
        self.targets, fingerprint, self.summary = load_combat_targets(directory)
        self.weight, self.maximum_ratio = weight, maximum_ratio
        self.identity = dict(labels_sha256=fingerprint, weight=weight, maximum_ratio=maximum_ratio)
        if saved is not None and saved != self.identity:
            raise ValueError("Combat resume requires the same labels, weight and gradient cap")

    def backward(self, model, scaler, cursor, update, microbatch):
        # This branch precedes sampling, packing, forward/backward and RNG calls.
        if self.weight == 0 or cursor.get("combat_distill_applied"):
            return None
        rng = random.Random(f"combat-distill:{update}")
        if len(self.targets) >= LABELS_PER_UPDATE:
            selected = rng.sample(self.targets, LABELS_PER_UPDATE)
        else:
            selected = [self.targets[rng.randrange(len(self.targets))] for _ in range(LABELS_PER_UPDATE)]
        parameters = list(model.parameters())
        gradients = [torch.zeros_like(parameter) for parameter in parameters]
        total = 0.
        for start in range(0, len(selected), microbatch):
            rows = selected[start:start + microbatch]
            inputs, teacher = combat_batch_targets(rows, parameters[0].device)
            with model.no_sync():
                logits, _ = model.module(inputs)
                loss = direct_policy_kl(logits, teacher, inputs["candidate_mask"])
                if not bool(torch.isfinite(loss)):
                    raise FloatingPointError("Nonfinite combat KL")
                values = torch.autograd.grad(scaler.scale(loss * self.weight * len(rows) / LABELS_PER_UPDATE),
                                             parameters, allow_unused=True)
            for accumulated, value in zip(gradients, values):
                if value is not None:
                    accumulated.add_(value.detach())
            total += float(loss.detach()) * len(rows) / LABELS_PER_UPDATE
        cursor["combat_distill_report"] = dict(loss=total, label_points=LABELS_PER_UPDATE,
                                               sampling_with_replacement=len(self.targets) < LABELS_PER_UPDATE,
                                               weight=self.weight, maximum_ratio=self.maximum_ratio, **self.summary)
        return torch.cat([gradient.reshape(-1) for gradient in gradients])

    def report(self, cursor):
        result = dict(cursor.get("combat_distill_report", {}))
        result.update(weight=self.weight, maximum_ratio=self.maximum_ratio, **self.summary)
        result.setdefault("label_points", 0)
        result.setdefault("distill_gradient_norm", 0.)
        result.setdefault("gradient_ratio_after_cap", 0.)
        totals = cursor.get("combat_ppo_totals")
        if totals is not None:
            kl, entropy, normalized, count = totals.tolist()
            result.update(combat_ppo_kl=kl / count if count else None,
                          combat_policy_entropy_nats=entropy / count if count else None,
                          combat_policy_normalized_entropy=normalized / count if count else None,
                          combat_ppo_rows=int(count), combat_ppo_kl_estimator="sampled_k3")
        return result
