"""Continue Stage12 with one running allocation and one budgeted successor."""
import argparse
from contextlib import contextmanager
import fcntl
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time

TERMINAL = {'COMPLETED', 'FAILED', 'CANCELLED', 'OUT_OF_MEMORY', 'TIMEOUT', 'NODE_FAIL', 'PREEMPTED'}


def command(*args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True)


def fields(text):
    return dict(part.split('=', 1) for part in text.split() if '=' in part)


def save(root, state, event):
    event = dict(at=time.time(), **event)
    state['updated_at'] = event['at']
    temporary = root / 'state.json.tmp'
    temporary.write_text(json.dumps(state, indent=2) + '\n')
    temporary.replace(root / 'state.json')
    with (root / 'events.jsonl').open('a') as stream:
        stream.write(json.dumps(event) + '\n')


@contextmanager
def locked(root):
    with (root / 'lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield json.loads((root / 'state.json').read_text())


def available_wall(state):
    unfinished = sum(job['wall_seconds'] * job['world'] / 8
                     for job in state['jobs'].values() if not job.get('settled'))
    remaining = state['limit_node_seconds'] - state['charged_node_seconds'] - unfinished
    return min(14400, int(remaining * 8 / state['request']['world']) // 60 * 60)


def settle(root, state):
    for identity, job in state['jobs'].items():
        if job.get('settled'):
            continue
        raw = command('sacct', '-j', identity, '-X', '--parsable2', '--noheader',
                      '--format=JobIDRaw,State,ExitCode,ElapsedRaw,Start,End')
        rows = [line.split('|') for line in raw.splitlines() if line.startswith(identity + '|')]
        if not rows:
            print(f'Accounting not visible yet: {identity}', file=sys.stderr, flush=True)
            continue
        row = rows[0]
        job['state'] = row[1].split()[0].rstrip('+')
        if job['state'] not in TERMINAL:
            continue
        elapsed = int(row[3])
        charge = elapsed * job['world'] / 8
        job.update(settled=True, exit_code=row[2], elapsed_seconds=elapsed,
                   charged_node_seconds=charge, final_sacct_original=raw)
        if 'reserved_node_seconds' in job:
            job['reserved_node_seconds'] = 0
        state['charged_node_seconds'] += charge
        memory = Path(state['training_root']) / f'memory-{identity}.json'
        if memory.exists():
            peak = json.loads(memory.read_text())['peak_bytes']
            state['request']['memory_mib'] = math.ceil(peak * 1.25 / 1048576)
        save(root, state, dict(event='settled_final_Slurm', job_id=int(identity),
                              charged_node_seconds=charge))


def snapshot(root, state, trigger):
    if time.time() < state['next_resource_check_at'] or trigger in state['resource_triggers']:
        return state.get('resources'), False
    state['resource_triggers'].append(trigger)
    state['last_resource_trigger'] = trigger
    state['next_resource_check_at'] = time.time() + 630
    save(root, state, dict(event='resource_query_claim', trigger=trigger))
    jobs = command('scontrol', 'show', 'jobs', '-o')
    pending = [fields(line) for line in jobs.splitlines() if line.startswith('JobId=')]
    pending = [job for job in pending if job.get('JobState') == 'PENDING'
               and job['UserId'].split('(')[0] != os.environ['USER']
               and job.get('Reason') in ('Resources', 'Priority')]
    result = dict(all_jobs_original=jobs, pending=pending, finished_at=time.time())
    state['resources'] = result
    state['next_resource_check_at'] = time.time() + 630
    save(root, state, dict(event='resource_query_completed', trigger=trigger, pending=pending))
    return result, True


def enqueue(root, parent):
    with locked(root) as state:
        settle(root, state)
        children = [identity for identity, job in state['jobs'].items()
                    if job.get('parent_job') == int(parent) and not job.get('settled')]
        if children:
            return int(children[0])
        wall = available_wall(state)
        if state.get('halted') or wall < 60:
            save(root, state, dict(event='no_successor', reason='health_stop_or_budget_exhausted'))
            return None
        request = state['request']
        waiting = [job['JobId'] for job in state.get('resources', {}).get('pending', [])]
        dependency = 'afterany:' + ':'.join([parent] + waiting)
        receipt = command('sbatch', '--no-requeue', '--dependency=' + dependency,
            '--begin=now+11minutes', '--nodes=1', '--ntasks=1',
            '--partition=' + os.environ['STAGE12_PARTITION'],
            '--nodelist=' + os.environ['STAGE12_NODE'],
            f"--gres=gpu:V100:{request['world']}", f"--cpus-per-task={request['cpus']}",
            f"--mem={request['memory_mib']}M",
            f'--time={wall // 3600:02}:{wall % 3600 // 60:02}:00',
            '--job-name=spire-e4-stage12-chain',
            '--export=ALL,STAGE12_WORLD=' + str(request['world']) + ',STAGE12_PARENT=' + parent,
            '--chdir=' + state['repository'], '--output=' + str(root / 'chain-%j.log'),
            '--signal=B:USR1@240', str(root / 'stage12-chain.sbatch'))
        identity = re.search(r'Submitted batch job ([0-9]+)', receipt)[1]
        state['jobs'][identity] = dict(job_id=int(identity), parent_job=int(parent), state='PENDING',
            world=request['world'], wall_seconds=wall, memory_mib=request['memory_mib'],
            cpus=request['cpus'], dependency=dependency, receipt_original=receipt,
            submitted_at=time.time(), settled=False)
        save(root, state, dict(event='successor_submitted', job_id=int(identity), parent_job=int(parent)))
        return int(identity)


def checkpoint_state(state):
    import torch
    path = Path(state['training_root']) / 'training/model.pt'
    checkpoint = torch.load(path, map_location='cpu', weights_only=False)
    rank = torch.load(path.parent / 'rank-0.pt', map_location='cpu', weights_only=False)
    cursor = rank['cursor']
    steps = [int(item['step']) for item in checkpoint['optimizer']['state'].values() if 'step' in item]
    return dict(state=checkpoint['state'], optimizer_steps=steps[0],
                partial_steps=cursor['optimizer_steps'] if cursor is not None else 0,
                partial_attempts=cursor['optimizer_attempts'] if cursor is not None else 0,
                partial_skips=cursor['amp_skipped_steps'] if cursor is not None else 0)


def watch(root, trigger):
    with locked(root) as state:
        settle(root, state)
        resources, queried = snapshot(root, state, trigger)
        if resources and resources['pending']:
            (Path(state['training_root']) / 'training/stop-requested').touch()
            for identity, job in state['jobs'].items():
                if job.get('settled') or job['state'] != 'PENDING':
                    continue
                dependency = 'afterany:' + ':'.join([str(job['parent_job'])] +
                    [other['JobId'] for other in resources['pending']])
                if dependency != job['dependency']:
                    command('scontrol', 'update', 'JobId=' + identity, 'Dependency=' + dependency)
                    job['dependency'] = dependency
            save(root, state, dict(event='yield_requested', pending=resources['pending']))
        return dict(state=state, queried=queried)


def run(root):
    identity = os.environ['SLURM_JOB_ID']
    with locked(root) as state:
        settle(root, state)
        state['jobs'][identity]['state'] = 'RUNNING'
        checkpoint = checkpoint_state(state)
        state['jobs'][identity]['resume'] = checkpoint
        reason = checkpoint['state']['stop_reason']
        if reason not in (None, 'allocation_end_or_yield'):
            state['halted'] = True
            save(root, state, dict(event='health_stop_preserved', job_id=int(identity), reason=reason))
            return 0
        save(root, state, dict(event='successor_started', job_id=int(identity)))
    result = watch(root, 'start-' + identity)
    enqueue(root, identity)
    state = result['state']
    if state['resources']['pending']:
        return 0
    output = Path(state['training_root']) / 'training'
    (output / 'stop-requested').unlink(missing_ok=True)
    receipt = fields(command('scontrol', 'show', 'job', identity, '-o'))
    from datetime import datetime
    deadline = datetime.fromisoformat(receipt['EndTime']).timestamp()
    environment = dict(os.environ, OMP_NUM_THREADS='1', MKL_NUM_THREADS='1', OPENBLAS_NUM_THREADS='1',
                       PYTHONPATH=state['build'] + ':' + state['repository'])
    with (output / f'job-{identity}.log').open('w') as stream:
        worker = subprocess.Popen([str(Path(sys.executable).parent / 'torchrun'), '--standalone',
            '--nproc_per_node=' + str(state['jobs'][identity]['world']), '-m', 'tools.e4_stage12',
            '--output', str(output), '--deadline', str(deadline)], cwd=state['repository'],
            env=environment, stdout=stream, stderr=subprocess.STDOUT)
        while worker.poll() is None:
            with locked(root) as current:
                due = time.time() >= current['next_resource_check_at']
            if due:
                watch(root, 'running-' + identity + '-' + str(time.time_ns()))
            time.sleep(5)
    return worker.returncode


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('enqueue', 'watch', 'run', 'reconcile'))
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--parent')
    parser.add_argument('--trigger')
    args = parser.parse_args()
    if args.action == 'enqueue':
        print(json.dumps(dict(successor=enqueue(args.root, args.parent))), flush=True)
    elif args.action == 'watch':
        print(json.dumps(watch(args.root, args.trigger)), flush=True)
    elif args.action == 'reconcile':
        with locked(args.root) as state:
            settle(args.root, state)
            print(json.dumps(state), flush=True)
    else:
        sys.exit(run(args.root))


if __name__ == '__main__':
    main()
