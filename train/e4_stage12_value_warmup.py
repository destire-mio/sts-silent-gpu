"""Value-only learner for standalone reward warmup."""
import torch

from train.e4_stage12 import finish_optimizer_step, local_minibatch
from train.model import packed_inputs


def advance_value_minibatch(policy, optimizer, scaler, cursor, rng, microbatch, *, split=False):
    fragment = cursor["fragment"]
    selected, count = local_minibatch(cursor, rank=0)
    optimizer.zero_grad(set_to_none=True)
    by_act = cursor.setdefault("value_warmup_act_totals",
                               torch.zeros((3, 5), dtype=torch.float64, device=selected.device))
    for ids in selected.split(microbatch):
        inputs = packed_inputs(fragment["data"], fragment["offsets"], ids, ids.device)
        with torch.no_grad():
            _, pooled = policy.encode(inputs)
        if split:
            predicted = policy.values(pooled, fragment["decision_types"][ids] == 0)
        else:
            predicted = policy.value(pooled).squeeze(-1)
        error = predicted - fragment["returns"][ids]
        loss = error.square()
        scaler.scale(loss.sum() / count).backward()
        with torch.no_grad():
            acts = fragment["value_acts"][ids]
            values = torch.stack((error.double(), loss.double(), torch.ones_like(error, dtype=torch.float64)))
            by_act.scatter_add_(1, acts.expand_as(values), values)
            loss_total = values[1].sum()
            cursor["totals"][1] += loss_total
            cursor["totals"][5] += loss_total
            cursor["totals"][6] += len(ids)
            cursor["finite"] &= torch.isfinite(loss).all()
    finish_optimizer_step(policy, optimizer, scaler, cursor, rng, rank=0)


def warmup_act_metrics(cursor):
    errors, squares, counts = cursor["value_warmup_act_totals"].tolist()
    return {str(act): dict(samples=int(count), mean_error=errors[act] / count if count else None,
                           mse=squares[act] / count if count else None)
            for act, count in enumerate(counts) if act or count}
