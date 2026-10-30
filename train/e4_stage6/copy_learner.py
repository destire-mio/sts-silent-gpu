from contextlib import nullcontext

import torch
import torch.distributed as dist

from train.model import distribution, packed_inputs, record_lengths
from train.e4_stage6.copy_tape import pack_trials


MINIBATCH = 8192
MICROBATCH = 256


def validate_training_fragment(mainline, arm):
    assert mainline['source_partition'] == 'train', 'Examination cannot enter learner'
    seeds = mainline['training_seeds']
    assert seeds.ndim == 1 and len(seeds) == len(mainline['actions']) and len(seeds) > 0
    assert bool(((seeds >= 1000000000) & (seeds < 2000000000)).all()), 'Dev seed cannot update any parameter'
    if arm == 'B':
        assert all(1000000000 <= group['seed'] < 2000000000 for group in mainline['combat_copies']), 'Dev combat copy cannot enter learner'


def combine_rows(mainline):
    count = len(mainline['actions'])
    device = mainline['actions'].device
    fields = ('actions', 'logs', 'values', 'returns', 'advantages', 'policy_mask', 'training_seeds')
    result = {name: mainline[name] for name in fields}
    result.update(data=mainline['data'][:int(mainline['offsets'][-1])],
                  offsets=mainline['offsets'], copy=torch.zeros(count, dtype=torch.bool, device=device),
                  imitation=mainline['imitation'] & ~mainline['combat'] & mainline['action_valid'])
    groups = mainline['combat_copies']
    if groups:
        copies = pack_trials(groups, device)
        length = len(copies['actions'])
        for name in fields:
            result[name] = torch.cat((result[name], copies[name].to(device)))
        result['offsets'] = torch.cat((result['offsets'],
            copies['offsets'][1:].to(device) + int(mainline['offsets'][-1])))
        result['data'] = torch.cat((result['data'], copies['data'].to(device)))
        result['copy'] = torch.cat((result['copy'], torch.ones(length, dtype=torch.bool, device=device)))
        result['imitation'] = torch.cat((result['imitation'], torch.zeros(length, dtype=torch.bool, device=device)))
    else:
        result['data'] = result['data'].clone()
    result['ppo'] = result['copy'].clone()
    result['source_partition'] = 'train'
    return result


def losses(logits, run_value, combat_value, inputs, rows, indices, valid, beta):
    policy, normalized_entropy = distribution(logits, inputs['candidate_mask'])
    entropy = policy.entropy()
    log_prob = policy.log_prob(rows['actions'][indices])
    log_ratio = log_prob - rows['logs'][indices]
    ratio = log_ratio.exp()
    advantage = rows['advantages'][indices]
    clipped = torch.minimum(ratio * advantage, ratio.clamp(.8, 1.2) * advantage)
    copy = rows['copy'][indices] & valid
    ppo = rows['ppo'][indices] & valid
    mainline = ~rows['copy'][indices] & valid
    imitation = rows['imitation'][indices] & valid
    actor_loss = -clipped * ppo - log_prob * imitation
    run_loss = (run_value - rows['returns'][indices]).square() * mainline
    combat_loss = (combat_value - rows['returns'][indices]).square() * copy
    entropy_mask = copy | imitation
    loss = (actor_loss + .5 * (run_loss + combat_loss) - beta * entropy * entropy_mask
            - .01 * normalized_entropy * (ppo & ~copy))
    return loss, dict(copy=copy, ppo=ppo, mainline=mainline, imitation=imitation,
        actor=actor_loss, run_value=run_loss, combat_value=combat_loss,
        entropy=entropy, normalized_entropy=normalized_entropy, entropy_mask=entropy_mask,
        log_ratio=log_ratio, ratio=ratio)


def padded_plan(count, maximum, rng, device):
    indices = torch.zeros(maximum, dtype=torch.int64, device=device)
    indices[:count] = torch.randperm(count, device=device, generator=rng)
    valid = torch.arange(maximum, device=device) < count
    return indices, valid


def begin_update(mainline, rng, epochs, arm):
    assert arm in ('A', 'B') and epochs > 0
    validate_training_fragment(mainline, arm)
    if arm == 'B':
        rows = combine_rows(mainline)
        adv, copy = rows['advantages'], rows['copy']
        moments = torch.stack((adv[copy].double().sum(), adv[copy].double().square().sum(), copy.sum().double()))
        dist.all_reduce(moments)
        mean = moments[0] / moments[2].clamp_min(1)
        std = (moments[1] / moments[2].clamp_min(1) - mean.square()).clamp_min(0).sqrt()
        rows['advantages'] = torch.where(copy, adv / (std + 1e-8), adv).float()
        copy_moments = moments.tolist()
    else:
        names = ('data', 'offsets', 'policy_mask', 'actions', 'logs', 'values', 'returns', 'advantages', 'training_seeds', 'source_partition')
        rows = {name: mainline[name] for name in names}
        rows['data'] = rows['data'][:int(rows['offsets'][-1])].clone()
        rows['copy'] = torch.zeros_like(mainline['action_valid'])
        rows['imitation'] = mainline['imitation'] & mainline['action_valid']
        rows['ppo'] = ~mainline['imitation'] & mainline['action_valid'] & mainline['combat']
        adv = rows['advantages']; ppo = rows['ppo']
        moments = torch.stack((adv[ppo].double().sum(), adv[ppo].double().square().sum(), ppo.sum().double()))
        dist.all_reduce(moments)
        mean = moments[0] / moments[2].clamp_min(1)
        std = (moments[1] / moments[2].clamp_min(1) - mean.square()).clamp_min(0).sqrt()
        rows['advantages'] = ((adv - mean) / (std + 1e-8)).float()
        copy_moments = None
    device = rows['actions'].device
    count = len(rows['actions'])
    maximum = torch.tensor(count, device=device)
    dist.all_reduce(maximum, op=dist.ReduceOp.MAX)
    order, valid = padded_plan(count, int(maximum), rng, device)
    return dict(rows=rows, count=count, maximum=int(maximum), order=order, valid=valid,
                epoch=0, start=0, epochs=epochs, arm=arm, done=False, metrics=None,
                total=torch.zeros(16, dtype=torch.float64, device=device), optimizer_steps=0,
                copy_advantage_moments_global=copy_moments)


def advance_update(model, optimizer, cursor, rng, beta):
    assert not cursor['done'] and cursor['metrics'] is None
    rows = cursor['rows']; world = dist.get_world_size(); device = rows['actions'].device
    assert rows['source_partition'] == 'train'
    assert bool(((rows['training_seeds'] >= 1000000000) & (rows['training_seeds'] < 2000000000)).all()), 'Dev seed in restored learner cursor'
    start = cursor['start']; per_rank = MINIBATCH // world
    selected = cursor['order'][start:start + per_rank]
    present = cursor['valid'][start:start + per_rank]
    global_count = present.sum().to(torch.float64)
    dist.all_reduce(global_count)
    assert int(global_count) > 0
    entities, candidates = record_lengths(rows['data'], rows['offsets'], selected)
    order = torch.argsort((entities // 16) * 129 + candidates, stable=True)
    selected, present = selected[order], present[order]
    chunks = list(range(0, len(selected), MICROBATCH))
    finite = torch.ones((), dtype=torch.bool, device=device)
    optimizer.zero_grad(set_to_none=True)
    for j, begin in enumerate(chunks):
        indices = selected[begin:begin + MICROBATCH]
        with model.no_sync() if j + 1 < len(chunks) else nullcontext():
            inputs = packed_inputs(rows['data'], rows['offsets'], indices, device)
            inputs['candidate_mask'] &= rows['policy_mask'][indices, :inputs['candidate_mask'].shape[1]]
            with torch.autocast(device_type=device.type, dtype=torch.bfloat16, enabled=device.type == 'cuda'):
                if cursor['arm'] == 'A':
                    logits, run_value = model(inputs)
                    combat_value = run_value * 0.
                else:
                    logits, run_value, combat_value = model(inputs)
            logits, run_value, combat_value = logits.float(), run_value.float(), combat_value.float()
            loss, metrics = losses(logits, run_value, combat_value, inputs, rows,
                                   indices, present[begin:begin + MICROBATCH], beta)
            finite &= torch.isfinite(loss).all()
            (loss.sum() * world / global_count).backward()
            cursor['total'] += torch.stack((metrics['actor'].detach().double().sum(),
                        metrics['run_value'].detach().double().sum(),
                        metrics['combat_value'].detach().double().sum(),
                        loss.detach().double().sum(), metrics['copy'].double().sum(),
                        metrics['mainline'].double().sum(), metrics['imitation'].double().sum(),
                        (metrics['entropy'].detach() * metrics['entropy_mask'] * (cursor['epoch'] == 0)).double().sum(),
                        (metrics['entropy_mask'] * (cursor['epoch'] == 0)).double().sum(),
                        ((metrics['ratio'] - 1 - metrics['log_ratio']).detach() * metrics['copy']).double().sum(),
                        (((metrics['ratio'] - 1).abs() > .2) * metrics['copy']).double().sum(),
                        present[begin:begin + MICROBATCH].double().sum(),
                        metrics['ppo'].double().sum(),
                        (metrics['normalized_entropy'].detach() * metrics['ppo']).double().sum(),
                        ((metrics['ratio'] - 1 - metrics['log_ratio']).detach() * metrics['ppo']).double().sum(),
                        (((metrics['ratio'] - 1).abs() > .2) * metrics['ppo']).double().sum()))
    assert bool(finite), 'Non-finite stage6 minibatch loss'
    torch.nn.utils.clip_grad_norm_(model.parameters(), .5, error_if_nonfinite=True)
    optimizer.step()
    cursor['optimizer_steps'] += 1
    cursor['start'] += len(selected)
    if cursor['start'] == cursor['maximum']:
        cursor['epoch'] += 1
        cursor['start'] = 0
        cursor['done'] = cursor['epoch'] == cursor['epochs']
        if not cursor['done']:
            cursor['order'], cursor['valid'] = padded_plan(cursor['count'], cursor['maximum'], rng, device)


def finish_update(cursor, beta):
    assert cursor['done'] and cursor['metrics'] is None
    total = cursor['total'].clone()
    dist.all_reduce(total)
    assert int(total[8]) > 0
    result = dict(actor_loss=float(total[0] / total[11]),
                run_value_loss=float(total[1] / total[5].clamp_min(1)),
                combat_value_loss=float(total[2] / total[4].clamp_min(1)),
                loss=float(total[3] / total[11]), copy_samples=int(total[4]),
                mainline_value_samples=int(total[5]), event_imitation_samples=int(total[6]),
                H_imit=float(total[7] / total[8]), imitation_entropy_samples=int(total[8]),
                entropy_scope='raw_nats_retained_combat_and_valid_noncombat_imitation_first_epoch',
                copy_kl=float(total[9] / total[4].clamp_min(1)),
                copy_clip_fraction=float(total[10] / total[4].clamp_min(1)), epochs=cursor['epochs'],
                beta_used=beta, trained_rows=int(total[11]), padding_rows_excluded=True,
                ppo_samples=int(total[12]), entropy=float(total[13] / total[12].clamp_min(1)),
                kl=float(total[14] / total[12].clamp_min(1)),
                clip_fraction=float(total[15] / total[12].clamp_min(1)), optimizer_steps=cursor['optimizer_steps'])
    if cursor['arm'] == 'A':
        result['imitation_samples'] = result.pop('event_imitation_samples')
        result['entropy_scope'] = 'raw_nats_valid_mainline_imitation_first_epoch'
    else:
        moments = cursor.get('copy_advantage_moments_global')
        result['copy_advantage_moments_global'] = moments
        result['copy_advantage_std_global'] = (max(0., moments[1] / max(1, moments[2])
            - (moments[0] / max(1, moments[2]))**2)**.5 if moments is not None else None)
        result['copy_advantage_std_scope'] = 'Global population before scaling; null for a cursor begun before Claude92'
    cursor['metrics'] = result
    return result
