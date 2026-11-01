"""First-resume value calibration and observational rollout statistics."""
import math
import torch
import torch.distributed as dist
from train.model import distribution


def encoder_parameters(policy):
    return [p for name,p in policy.named_parameters()
            if name == 'cls' or name.startswith(('numbers.', 'tags.', 'ids.', 'input_norm.', 'encoder.'))]


def gradient_measurement(policy, inputs, actions, old_logs, advantages, returns, scale=1.):
    logits, value = policy(inputs)
    probability, _ = distribution(logits.float(), inputs['candidate_mask'])
    ratio = (probability.log_prob(actions) - old_logs).exp()
    actor = -torch.minimum(ratio * advantages, ratio.clamp(.8,1.2) * advantages).mean()
    value_loss = .5 * (value - returns).square().mean()
    parameters = encoder_parameters(policy)
    actor_grad = torch.autograd.grad(actor * scale, parameters, retain_graph=True)
    value_grad = torch.autograd.grad(value_loss * scale, parameters)
    norm = lambda gradients:torch.stack([g.detach().double().square().sum() for g in gradients]).sum().sqrt() / scale
    actor_norm, value_norm = float(norm(actor_grad)), float(norm(value_grad))
    ratio = value_norm / actor_norm if actor_norm else (math.inf if value_norm else 0.)
    target = min(200., max(.5, .25 / ratio)) if ratio else 200.
    return dict(actor_encoder_norm=actor_norm, weighted_value_encoder_norm=value_norm,
                measured_value_coefficient=.5, ratio=ratio, target=target, completed_updates=0)


def value_coefficient(state):
    calibration = state.get('value_calibration')
    if calibration is None:
        return .5
    fraction = min(20, calibration['completed_updates']) / 20
    return .5 + (calibration['target'] - .5) * fraction


def explained_variance(fragment):
    valid = fragment['action_valid']
    result = []
    for kind in [-1,0,1,2,3,4,5]:
        selected = valid if kind < 0 else valid & (fragment['decision_types'] == kind)
        returns = fragment['returns'][selected].double()
        error = returns - fragment['values'][selected].double()
        totals = torch.stack((returns.new_tensor(len(returns)), returns.sum(), returns.square().sum(),
                              error.sum(), error.square().sum()))
        dist.all_reduce(totals)
        n, total, squares, err, err_squares = totals.tolist()
        variance = squares / n - (total / n)**2 if n else 0.
        result.append(dict(kind=kind, samples=int(n), explained_variance=
            1 - (err_squares / n - (err / n)**2) / variance if variance > 0 else None))
    return result


def nonforced_entropy(totals, names):
    rows = []
    for i,name in enumerate(names):
        entropy, nonforced, forced = (float(totals[j,i]) for j in range(3))
        rows.append(dict(kind=name, normalized_entropy_nonforced=entropy/nonforced if nonforced else None,
                         nonforced_count=int(nonforced), forced_count=int(forced),
                         forced_fraction=forced/(forced+nonforced) if forced+nonforced else None))
    return rows
