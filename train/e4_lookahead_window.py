"""One dev-60 comparison in natural allocation evaluation windows."""
import hashlib
import json
import math
from pathlib import Path
import statistics
import time
import torch
import torch.distributed as dist
from train.e4 import AllocationEnded
from train.e4_stage12 import CLSPolicy, NoSLSearch
from train.e4_evaluation_ledger import EvaluationLedgerCollector
from train.e4_lookahead import LookaheadSearch


COUNTERS = ('mainline_decisions','preview_decisions','overrides','preview_steps')


def write_json(path, value):
    temporary = Path(str(path) + '.tmp')
    temporary.write_text(json.dumps(value,allow_nan=False) + '\n')
    temporary.replace(path)


def summary(rows, baseline):
    previous = {e['seed']:e['floor'] for e in baseline}
    delta = [e['floor']-previous[e['seed']] for e in rows]
    return dict(episodes=len(rows),mean_floor=statistics.mean(e['floor'] for e in rows),
        act1_boss=sum(e['reach']['act1_boss'] for e in rows),
        act2_boss=sum(e['reach']['act2_boss'] for e in rows),
        act3_first_boss=sum(e['reach']['act3_first_boss'] for e in rows),
        victories=sum(e['reach']['act3_second_boss'] for e in rows),
        heart_wins=sum(e['reach']['heart'] for e in rows),
        paired_mean_delta=statistics.mean(delta),
        paired_SE=statistics.stdev(delta)/math.sqrt(len(delta)))


def combat_estimate(turn_seconds):
    return 4 * turn_seconds


def window_stop_requested(output, deadline):
    return (Path(output)/'stop-requested').exists() or time.monotonic() >= deadline


class ArgmaxSearch(NoSLSearch):
    def __init__(self, lanes, device):
        super().__init__(lanes,device,130,greedy=True)
        for name in COUNTERS:setattr(self,name,0)

    def predict(self, model, slots):
        result = super().predict(model,slots)
        self.mainline_decisions += len(slots)
        return result


class WindowCollector(EvaluationLedgerCollector):
    def advance(self, slots, actions, progress):
        count = len(self.completed)
        result = super().advance(slots,actions,progress)
        if len(self.completed) != count:self.persist()
        return result


def lookahead60_window(output, config, device, rank, world, deadline):
    output = Path(output)
    checkpoint = output/'dev-60.pt'
    directory = output/'claude137-dev60'
    if not checkpoint.exists() or (directory/'report.json').exists():return True
    assert world == 8
    directory.mkdir(exist_ok=True)
    torch.cuda.synchronize(device)
    with torch.random.fork_rng(devices=[device]):
        policy = CLSPolicy(config).to(device)
        policy.load_state_dict(torch.load(checkpoint,map_location=device,weights_only=True),strict=True)
        policy.encoder_fp16 = True
        policy.eval().requires_grad_(False)
        for mode in ('argmax','turn','combat'):
            stop = torch.tensor(int(window_stop_requested(output,deadline)),device=device)
            dist.all_reduce(stop,op=dist.ReduceOp.MAX)
            if bool(stop):return False
            mode_path = directory/(mode+'.json')
            mode_record = json.loads(mode_path.read_text()) if mode_path.exists() else dict(seconds=0.,complete=False)
            if mode_record['complete'] or mode_record.get('skipped'):continue
            if mode == 'combat':
                turn = json.loads((directory/'turn.json').read_text())
                estimate = combat_estimate(turn['seconds'])
                if estimate > 3600:
                    if rank == 0:write_json(mode_path,dict(skipped='estimated_over_one_hour',estimated_seconds=estimate,complete=False))
                    dist.barrier()
                    continue
            torch.cuda.synchronize(device)
            started = time.monotonic()
            path = directory/f'{mode}-rank-{rank}.json'
            saved = json.loads(path.read_text()) if path.exists() else dict(episodes=[],counters=dict.fromkeys(COUNTERS,0))
            completed = {e['seed'] for e in saved['episodes']}
            seeds = list(range(2000000000,2000000256))[rank::world]
            pending = [seed for seed in seeds if seed not in completed]
            complete = True
            if pending:
                search = ArgmaxSearch(32,device) if mode == 'argmax' else LookaheadSearch(32,device,130,depth=mode)
                search.deadline = min(deadline,started+3600-mode_record['seconds']) if mode == 'combat' else deadline
                search.evidence_directory = directory
                original_clock = search.clock
                def clock():
                    original_clock()
                    if window_stop_requested(output,search.deadline):raise AllocationEnded
                search.clock = clock
                for name in COUNTERS:setattr(search,name,saved['counters'][name])
                if mode != 'argmax' and 'preview_rng' in saved:
                    search.lookahead.rng.set_state(torch.tensor(saved['preview_rng'],dtype=torch.uint8))
                collector = WindowCollector(search,torch.Generator().manual_seed(130),rank=rank)
                previous = saved['episodes']
                def persist():
                    rows = previous + [{k:e[k] for k in ('seed','floor','reach','keys','outcome','reward')}
                                       for e in collector.completed]
                    record = dict(episodes=rows,counters={name:getattr(search,name) for name in COUNTERS})
                    if mode != 'argmax':record['preview_rng'] = search.lookahead.rng.get_state().tolist()
                    write_json(path,record)
                collector.persist = persist
                try:
                    collector.evaluate(policy,pending)
                except AllocationEnded:
                    complete = False
                persist()
                del collector, search
                torch.cuda.empty_cache()
            torch.cuda.synchronize(device)
            elapsed = torch.tensor(time.monotonic()-started,device=device,dtype=torch.float64)
            dist.all_reduce(elapsed,op=dist.ReduceOp.MAX)
            flag = torch.tensor(int(complete),device=device)
            dist.all_reduce(flag,op=dist.ReduceOp.MIN)
            total_seconds = mode_record['seconds']+float(elapsed)
            if rank == 0:
                write_json(mode_path,dict(complete=bool(flag),seconds=total_seconds,
                    skipped='combat_one_hour_cap' if mode=='combat' and not bool(flag) and total_seconds>=3600 else None))
            dist.barrier()
            if not bool(flag):
                if mode == 'combat' and total_seconds >= 3600:continue
                return False
        if rank == 0:
            baseline = []
            for owner in range(world):baseline.extend(json.loads((directory/f'argmax-rank-{owner}.json').read_text())['episodes'])
            assert sorted(e['seed'] for e in baseline)==list(range(2000000000,2000000256))
            report = dict(checkpoint='dev-60.pt',checkpoint_sha256=hashlib.sha256(checkpoint.read_bytes()).hexdigest(),
                episodes=256,parameter_updates=0,evaluation_only=True,
                timing_scope='sum of max-rank wall durations per natural-allocation attempt; includes mode setup',
                combat_estimate_rule='4 times measured turn wall time (256/64 preview step caps)',modes={})
            for mode in ('argmax','turn','combat'):
                row = json.loads((directory/(mode+'.json')).read_text())
                if row['complete']:
                    episodes,counts = [],dict.fromkeys(COUNTERS,0)
                    for owner in range(world):
                        data = json.loads((directory/f'{mode}-rank-{owner}.json').read_text())
                        episodes.extend(data['episodes'])
                        for name in COUNTERS:counts[name] += data['counters'][name]
                    assert sorted(e['seed'] for e in episodes)==list(range(2000000000,2000000256))
                    row.update(summary(episodes,baseline),counters=counts,
                        preview_decision_fraction=counts['preview_decisions']/counts['mainline_decisions'],
                        override_fraction=counts['overrides']/counts['mainline_decisions'],
                        override_fraction_of_previews=counts['overrides']/counts['preview_decisions'] if counts['preview_decisions'] else 0.)
                report['modes'][mode] = row
            write_json(directory/'report.json',report)
        dist.barrier()
    torch.cuda.empty_cache()
    return True
