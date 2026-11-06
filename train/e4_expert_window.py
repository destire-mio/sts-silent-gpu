"""Periodic paired argmax/combat/card exams, isolated from training and long NCCL waits."""
import hashlib
import json
import time
from pathlib import Path
import torch
import torch.distributed as dist
from train.e4 import AllocationEnded
from train.e4_stage12 import CLSPolicy
from train.e4_lookahead import LookaheadSearch, CardLookaheadSearch
from train.e4_lookahead_window import (ArgmaxSearch, WindowCollector, COUNTERS,
                                      summary, write_json, window_stop_requested)


COMPARISON_EPISODES = 128


def due_combat_comparison(training_seconds):
    return int(training_seconds // 7200) > 0 and int(training_seconds // 7200) % 4 == 0


def complete_seed_set(rows, owner=None):
    wanted = list(range(2000000000, 2000000000 + COMPARISON_EPISODES))
    if owner is not None:
        wanted = wanted[owner::8]
    return sorted(e['seed'] for e in rows) == wanted


def paired_completed_summary(baseline, combat, mode='combat'):
    previous = {row['seed']: row for row in baseline}
    current = {row['seed']: row for row in combat}
    wanted = set(range(2000000000, 2000000000 + COMPARISON_EPISODES))
    assert len(previous) == len(baseline) and previous.keys() <= wanted
    assert len(current) == len(combat) and current.keys() <= wanted
    seeds = sorted(previous.keys() & current.keys())
    result = dict(episodes=len(seeds), excluded_seeds=sorted(wanted-set(seeds)), modes={})
    if not seeds:
        return result
    matched = [previous[seed] for seed in seeds]
    for name, source in (('argmax', previous), (mode, current)):
        rows = [source[seed] for seed in seeds]
        if len(seeds) > 1:
            result['modes'][name] = summary(rows, matched)
        else:
            # A single completed pair has a mean difference but no sample SE.
            row = rows[0]
            result['modes'][name] = dict(episodes=1, mean_floor=row['floor'],
                act1_boss=int(row['reach']['act1_boss']),
                act2_boss=int(row['reach']['act2_boss']),
                act3_first_boss=int(row['reach']['act3_first_boss']),
                victories=int(row['reach']['act3_second_boss']),
                heart_wins=int(row['reach']['heart']),
                paired_mean_delta=row['floor']-matched[0]['floor'], paired_SE=None)
    return result


def mode_window_seconds(records):
    attempts = set().union(*(set(r.get('windows', {})) for r in records))
    seconds = 0.
    for attempt in attempts:
        workers = {}
        for owner, record in enumerate(records):
            window = record.get('windows', {}).get(attempt, {})
            worker = window.get('worker_rank', owner)
            workers[worker] = workers.get(worker, 0.) + window.get('seconds', 0.)
        seconds += max(workers.values())
    return seconds


def wait_files(paths):
    # Every worker checks the original allocation clock while playing. Early finishers
    # wait on small receipts, not a NCCL collective that can time out during combat.
    while not all(path.exists() for path in paths):
        time.sleep(.2)


def periodic_combat_window(output, config, device, rank, world, deadline):
    output = Path(output)
    pending_path = output/'expert-pending-0.json'
    hour = -1
    if rank == 0 and pending_path.exists():
        hour = json.loads(pending_path.read_text())['hour']
    selection = torch.tensor(hour, dtype=torch.int64, device=device)
    dist.broadcast(selection, src=0)
    hour = int(selection)
    if hour < 0:
        return True
    assert 1 <= world <= 8 and hour % 8 == 0
    checkpoint = output/f'dev-{hour}.pt'
    directory = output/f'claude138-dev{hour}'
    directory.mkdir(exist_ok=True)
    if (directory/'report.json').exists():
        return True
    if window_stop_requested(output, deadline):
        return False
    # All ranks enter this short broadcast together, before any independent games.
    token = torch.tensor(time.time_ns() if rank == 0 else 0, dtype=torch.int64, device=device)
    dist.broadcast(token, src=0)
    token = int(token)
    sha = hashlib.sha256(checkpoint.read_bytes()).hexdigest()
    modes = ('argmax', 'combat', 'card') if 88 <= hour < 104 else ('argmax', 'combat')
    with torch.random.fork_rng(devices=[device.index]):
        for mode in modes:
            counters = COUNTERS + ('card_reward_decisions',) if mode == 'card' else COUNTERS
            mode_path = directory/f'{mode}.json'
            old = json.loads(mode_path.read_text()) if mode_path.exists() else dict(seconds=0., complete=False)
            if old.get('complete') or old.get('skipped'):
                continue
            mode_started = time.monotonic()
            for owner in range(rank, 8, world):
                path = directory/f'{mode}-rank-{owner}.json'
                saved = json.loads(path.read_text()) if path.exists() else dict(episodes=[], counters=dict.fromkeys(counters,0))
                assert saved.get('checkpoint_sha256', sha) == sha
                seen = {e['seed'] for e in saved['episodes']}
                seeds = list(range(2000000000,2000000000 + COMPARISON_EPISODES))[owner::8]
                assert len(seen) == len(saved['episodes']) and seen <= set(seeds)
                pending = [seed for seed in seeds if seed not in seen]
                started = time.monotonic()
                attempt_path = directory/f'{mode}-window-{token}-rank-{owner}.json'
                complete = not pending
                policy = search = collector = None
                final = False
                failed = False
                previous_windows = saved.get('windows', {})
                def persist():
                    episodes = saved['episodes'] + ([] if collector is None else [
                        {k:e[k] for k in ('seed','floor','reach','keys','outcome','reward')}
                        for e in collector.completed])
                    elapsed = time.monotonic()-started
                    record = dict(episodes=episodes,
                        counters=saved['counters'] if search is None else
                            {name:getattr(search,name) for name in counters},
                        checkpoint_sha256=sha, windows=dict(previous_windows))
                    record['windows'][str(token)] = dict(seconds=elapsed, final=final, failed=failed, worker_rank=rank)
                    if search is not None and mode != 'argmax':
                        record['preview_rng'] = search.lookahead.rng.get_state().tolist()
                    elif 'preview_rng' in saved:
                        record['preview_rng'] = saved['preview_rng']
                    write_json(path, record)
                    return elapsed
                try:
                    if pending:
                        policy = CLSPolicy(config).to(device)
                        policy.load_state_dict(torch.load(checkpoint,map_location=device,weights_only=True),strict=True)
                        policy.encoder_fp16 = True
                        policy.eval().requires_grad_(False)
                        search = (ArgmaxSearch(32,device) if mode == 'argmax' else
                                  CardLookaheadSearch(32,device,130) if mode == 'card' else
                                  LookaheadSearch(32,device,130,depth='combat'))
                        search.deadline = min(deadline,mode_started+max(0.,3600-old['seconds']))
                        search.evidence_directory = directory
                        for name in counters:
                            setattr(search,name,saved['counters'][name])
                        if mode != 'argmax' and 'preview_rng' in saved:
                            search.lookahead.rng.set_state(torch.tensor(saved['preview_rng'],dtype=torch.uint8))
                        collector = WindowCollector(search,torch.Generator().manual_seed(130),rank=owner)
                        collector.persist = persist
                        original_clock = search.clock
                        last_persist = [started]
                        def clock():
                            original_clock()
                            now = time.monotonic()
                            if now-last_persist[0] >= 10:
                                persist()
                                last_persist[0] = now
                            if window_stop_requested(output, search.deadline):
                                raise AllocationEnded()
                        search.clock = clock
                        collector.evaluate(policy, pending)
                        complete = True
                except AllocationEnded:
                    complete = False
                except BaseException:
                    failed = True
                    raise
                finally:
                    torch.cuda.synchronize(device)
                    final = True
                    elapsed = persist()
                    write_json(attempt_path,dict(seconds=elapsed, complete=complete, failed=failed))
                    del collector, search, policy
                    torch.cuda.empty_cache()
            paths = [directory/f'{mode}-window-{token}-rank-{owner}.json' for owner in range(8)]
            wait_files(paths)
            if rank == 0:
                windows = [json.loads(p.read_text()) for p in paths]
                # Reconstruct any earlier crash prefix from durable per-rank timer receipts.
                records = [json.loads((directory/f'{mode}-rank-{owner}.json').read_text()) for owner in range(8)]
                attempts = set().union(*(set(r.get('windows',{})) for r in records))
                seconds = mode_window_seconds(records)
                timing_complete = all(all(at in r.get('windows',{}) and r['windows'][at]['final']
                                          and not r['windows'][at]['failed'] for r in records) for at in attempts)
                full = all(complete_seed_set(r['episodes'], owner) for owner,r in enumerate(records))
                row = dict(complete=full, seconds=seconds, timing_complete=timing_complete,
                    timing_scope='sum of maximum per-rank mode wall per natural window, including initialization',
                    timer_failure_scope='non-final crash prefixes are durable lower bounds, not exact total wall',
                    skipped=f'{mode}_one_hour_cap' if not full and seconds>=3600 else None)
                write_json(mode_path,row)
                write_json(directory/f'{mode}-ready-{token}.json',row)
            ready = directory/f'{mode}-ready-{token}.json'
            wait_files([ready])
            status = json.loads(ready.read_text())
            if not status['complete'] and not status.get('skipped'):
                return False
        if rank == 0:
            baseline = sum((json.loads((directory/f'argmax-rank-{owner}.json').read_text())['episodes']
                            for owner in range(8)),[])
            combat_rows = sum((json.loads((directory/f'combat-rank-{owner}.json').read_text())['episodes']
                               for owner in range(8)),[])
            report = dict(checkpoint=checkpoint.name, checkpoint_sha256=sha, episodes=COMPARISON_EPISODES,
                paired_completed=paired_completed_summary(baseline, combat_rows),
                evaluation_only=True,parameter_updates=0,modes={})
            if 'card' in modes and hour >= 96:
                report['card_randomization'] = 'card 已用整局随机化（#157）'
            completed = {}
            for mode in modes:
                status = json.loads((directory/f'{mode}.json').read_text())
                records = [json.loads((directory/f'{mode}-rank-{owner}.json').read_text()) for owner in range(8)]
                rows = sum((r['episodes'] for r in records),[])
                completed[mode] = rows
                if status['complete']:
                    assert complete_seed_set(rows)
                counters = COUNTERS + ('card_reward_decisions',) if mode == 'card' else COUNTERS
                counts = {name:sum(r['counters'][name] for r in records) for name in counters}
                status.update(completed_episodes=len(rows), counters=counts,
                    preview_decision_fraction=counts['preview_decisions']/max(1,counts['mainline_decisions']),
                    override_fraction=counts['overrides']/max(1,counts['mainline_decisions']),
                    override_fraction_of_previews=counts['overrides']/max(1,counts['preview_decisions']))
                matched = paired_completed_summary(baseline, rows, mode)
                if matched['episodes']:
                    status.update(matched['modes'][mode])
                report['modes'][mode] = status
            report['paired_completed_by_mode'] = {
                mode:paired_completed_summary(baseline, completed[mode], mode) for mode in modes[1:]}
            common = set.intersection(*({row['seed'] for row in completed[mode]} for mode in modes))
            report['common_completed'] = dict(episodes=len(common),
                excluded_seeds=sorted(set(range(2000000000,2000000000+COMPARISON_EPISODES))-common),
                modes={mode:paired_completed_summary(
                    [row for row in baseline if row['seed'] in common],
                    [row for row in completed[mode] if row['seed'] in common], mode)['modes']
                    for mode in modes[1:]})
            write_json(directory/'report.json',report)
        wait_files([directory/'report.json'])
    return True
