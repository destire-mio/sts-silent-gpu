"""Opt-in Stage12 standalone PPO targets and calibration."""
import torch
from torch import nn
from torch.nn import functional as F

from train.e4 import ACT, BATTLE_OVER, COMBAT_PHASE, DEATH, FLOOR, HP, PHASE, SCREEN, SMOKED
from train.e4_stage12 import CLSPolicy, GAE_LAMBDA

MILESTONES = ("act1_boss", "act2_boss", "act3_first_boss", "act4", "heart")
OUTPUTS = {"combat": 2, "global": len(MILESTONES) + 1}


class AuxiliaryPolicy(CLSPolicy):
    def install_auxiliary_heads(self, weights):
        width = self.config.width
        self.auxiliary_heads = nn.ModuleDict({
            name: nn.Sequential(nn.Linear(width, 128), nn.GELU(), nn.Linear(128, OUTPUTS[name]))
            for name in weights
        })

    def forward(self, observation, *, auxiliary=False):
        if not auxiliary:
            return super().forward(observation)
        entities, pooled = self.encode(observation)
        return (self.action_logits(observation, entities, pooled), self.value(pooled).squeeze(-1),
                {name: head(pooled) for name, head in self.auxiliary_heads.items()})


def rollout_storage(weights, length, lanes, device):
    result = {}
    for name in weights:
        shape = (length, lanes, OUTPUTS[name])
        result[name] = dict(logits=torch.zeros(shape, device=device),
                            eligible=torch.zeros(shape, dtype=torch.bool, device=device),
                            resolved=torch.zeros(shape, dtype=torch.bool, device=device),
                            outcome=torch.zeros(shape, device=device))
    if "combat" in result:
        result["combat"]["max_hp"] = torch.zeros(lanes, device=device)
    return result


def player_max_hp(search, slots):
    player = search.arrays["tags"][slots, 0, :] == 2
    return search.arrays["values"][slots, 1, :].masked_fill(~player, 0).amax(dim=-1).float()


class Capture:
    def __init__(self, model):
        self.model = model
        self.outputs = {}

    def __call__(self, inputs):
        logits, values, auxiliary = self.model(inputs, auxiliary=True)
        for name, prediction in auxiliary.items():
            self.outputs.setdefault(name, []).append(prediction.detach())
        return logits, values


def eligibility(name, collector, slots, progress):
    if name == "combat":
        return [[progress[slot][PHASE] == COMBAT_PHASE] * 2 for slot in slots]
    return [[not collector.rewards[slot].reach[key] for key in MILESTONES] + [True] for slot in slots]


def record_decision(storage, collector, model, slots, progress, tick):
    capture = Capture(model)
    decision = collector.choose(capture, slots, progress)
    valid = [slot for slot in slots if slot not in collector.search.failed]
    if not valid:
        return decision
    device = collector.search.device
    for name, rows in storage.items():
        logits = torch.cat(capture.outputs[name])
        rows["logits"][tick, valid] = logits
        rows["eligible"][tick, valid] = torch.tensor(eligibility(name, collector, valid, progress), device=device)
        if name == "combat":
            active = [slot for slot in valid if progress[slot][PHASE] == COMBAT_PHASE]
            if active:
                rows["max_hp"][active] = player_max_hp(collector.search, active)
        if name == "global":
            starts = [(i, slot) for i, slot in enumerate(valid)
                      if collector.starts[slot] == "fresh" and not collector.paths[slot]]
            if starts:
                probabilities = logits[[i for i, _ in starts], :-1].sigmoid().cpu().tolist()
                fresh = collector.auxiliary_fresh
                for (_, slot), row in zip(starts, probabilities):
                    fresh["pending"][slot] = row
                    fresh["started"].append(row)
    return decision


def record_outcome(storage, collector, slots, progress, terminal, tick):
    search = collector.search
    valid = [slot for slot in slots if slot not in collector.stalled]
    ends = {slot for slot, end in zip(slots, terminal) if end}
    after = search.last_step_progress
    for rows in storage.values():
        if collector.stalled:
            rows["eligible"][tick, sorted(collector.stalled)] = False
    if "combat" in storage:
        combat = storage["combat"]
        ended = [slot for slot in valid if progress[slot][PHASE] == COMBAT_PHASE
                 and (slot in ends or after[slot][PHASE] != COMBAT_PHASE
                      or (after[slot][ACT], after[slot][FLOOR]) != (progress[slot][ACT], progress[slot][FLOOR]))]
        if ended:
            alive = [slot for slot in ended if after[slot][SCREEN] != DEATH]
            hp_fraction = dict.fromkeys(ended, 0.)
            if alive:
                max_hp = player_max_hp(search, alive)
                max_hp = torch.where(max_hp > 0, max_hp, combat["max_hp"][alive])
                for slot, hp in zip(alive, max_hp.cpu().tolist()):
                    if hp > 0:
                        hp_fraction[slot] = after[slot][HP] / hp
                    else:
                        combat["eligible"][tick, slot, 1] = False
            won = [bool(after[slot][BATTLE_OVER]) and not after[slot][SMOKED]
                   and after[slot][HP] > 0 and after[slot][SCREEN] != DEATH for slot in ended]
            combat["resolved"][tick, ended] = True
            combat["outcome"][tick, ended] = torch.tensor(
                list(zip(won, (hp_fraction[slot] for slot in ended))), device=search.device)
    if "global" in storage:
        global_rows = storage["global"]
        reached = [[bool(collector.rewards[slot].reach[key]) for key in MILESTONES] for slot in valid]
        if valid:
            global_rows["resolved"][tick, valid] = torch.tensor(
                [[hit or slot in ends for hit in hits] + [slot in ends] for slot, hits in zip(valid, reached)],
                device=search.device)
            global_rows["outcome"][tick, valid] = torch.tensor(
                [hits + [after[slot][FLOOR] / 55.] for slot, hits in zip(valid, reached)], device=search.device)
        fresh = collector.auxiliary_fresh
        for slot in sorted(ends):
            prediction = fresh["pending"].pop(slot, None)
            if prediction is not None and slot not in collector.stalled:
                fresh["finished"].append((prediction, [bool(collector.rewards[slot].reach[key]) for key in MILESTONES]))


@torch.no_grad()
def lambda_targets(rows, endpoint, live):
    """Use observed outcomes when known, TD(lambda) for open trajectories."""
    logits, eligible = rows["logits"], rows["eligible"]
    targets = torch.empty_like(logits)
    valid = torch.empty_like(eligible)
    observed = torch.empty_like(eligible)
    next_prediction = target = endpoint.detach()
    continuation = live
    hard = torch.zeros_like(live)
    for tick in reversed(range(len(logits))):
        resolved = rows["resolved"][tick]
        target = torch.where(resolved, rows["outcome"][tick],
                             torch.where(hard, target, (1 - GAE_LAMBDA) * next_prediction + GAE_LAMBDA * target))
        continuation = (resolved | continuation) & eligible[tick]
        hard = (resolved | hard) & continuation
        targets[tick], valid[tick], observed[tick] = target, continuation, hard
        next_prediction = logits[tick].sigmoid()
    return targets, valid, observed


def roc_auc(probability, target):
    positive = int(target.sum())
    negative = len(target) - positive
    if not positive or not negative:
        return None
    ordered, order = probability.sort()
    _, groups = torch.unique_consecutive(ordered, return_inverse=True)
    negatives = torch.zeros_like(ordered)
    negatives.scatter_add_(0, groups, 1. - target[order])
    return float((target[order] * (negatives.cumsum(0) - .5 * negatives)[groups]).sum() / (positive * negative))


def mean(values):
    return float(values.mean()) if values.numel() else None


def binary_metrics(logits, targets, valid, observed):
    probabilities = logits[observed].sigmoid()
    outcomes = targets[observed]
    return dict(bce=mean(F.binary_cross_entropy_with_logits(logits[valid], targets[valid], reduction="none")),
                brier=mean((probabilities - outcomes).square()),
                target_brier=mean((logits[valid].sigmoid() - targets[valid]).square()),
                labels=int(valid.sum()), observed=int(observed.sum()))


def finish_rollout(storage, collector, model, live, progress, bootstrap):
    capture = Capture(model)
    if live:
        scores = collector.search.scores(capture, live, progress)
        bootstrap[live] = torch.tensor([scores[slot] for slot in live], device=bootstrap.device)
    fragment = {}
    metrics = {}
    for name, rows in storage.items():
        endpoint = torch.zeros_like(rows["logits"][0])
        eligible = torch.zeros_like(rows["eligible"][0])
        if live:
            endpoint[live] = torch.cat(capture.outputs[name]).sigmoid()
            eligible[live] = torch.tensor(eligibility(name, collector, live, progress), device=bootstrap.device)
        targets, valid, observed = lambda_targets(rows, endpoint, eligible)
        fragment[name] = dict(target=targets.flatten(0, 1), valid=valid.flatten(0, 1))
        logits = rows["logits"]
        mse = mean((logits[..., -1][valid[..., -1]].sigmoid() - targets[..., -1][valid[..., -1]]).square())
        if name == "combat":
            known = observed[..., 0]
            metrics[name] = dict(**binary_metrics(logits[..., 0], targets[..., 0], valid[..., 0], known),
                eligible=int(rows["eligible"][..., 0].sum()),
                coverage=float(valid[..., 0].float().mean()),
                combat_coverage=float(valid[..., 0].sum() / rows["eligible"][..., 0].sum().clamp_min(1)),
                auc=roc_auc(logits[..., 0][known].sigmoid(), targets[..., 0][known]), hp_mse=mse)
        else:
            fresh = collector.auxiliary_fresh
            starts, finished = fresh["started"], fresh["finished"]
            metrics[name] = dict(floor_mse=mse, fresh_starts=len(starts), fresh_completed=len(finished),
                                 fresh_pending=len(fresh["pending"]), milestones={})
            for i, key in enumerate(MILESTONES):
                metrics[name]["milestones"][key] = dict(
                    **binary_metrics(logits[..., i], targets[..., i], valid[..., i], observed[..., i]),
                    fresh_first_probability=sum(row[i] for row in starts) / len(starts) if starts else None,
                    fresh_completed_first_probability=sum(row[0][i] for row in finished) / len(finished) if finished else None,
                    fresh_actual_hit_rate=sum(row[1][i] for row in finished) / len(finished) if finished else None)
            fresh["started"], fresh["finished"] = [], []
    return fragment, metrics


def auxiliary_loss(predictions, fragment, ids, weights):
    loss = None
    for name, logits in predictions.items():
        target = fragment[name]["target"][ids]
        valid = fragment[name]["valid"][ids]
        binary = F.binary_cross_entropy_with_logits(logits[:, :-1], target[:, :-1], reduction="none")
        binary = (binary * valid[:, :-1]).sum(dim=1) / valid[:, :-1].sum(dim=1).clamp_min(1)
        regression = (logits[:, -1].sigmoid() - target[:, -1]).square() * valid[:, -1]
        weighted = weights[name] * (binary + regression)
        loss = weighted if loss is None else loss + weighted
    return loss
