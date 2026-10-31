"""Keep one budgeted Stage12 allocation and one afterany successor."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
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


def reservation(state):
    return sum(job['reserved_node_seconds'] for job in state['jobs'].values())


def available_wall(state):
    remaining = state['limit_node_seconds'] - state['charged_node_seconds'] - reservation(state)
    return min(14400, int(remaining * 8 / state['request']['world']) // 60 * 60)


def settle(root, state):
    changed = False
    for identity, job in state['jobs'].items():
        if job.get('settled'):
            continue
        raw = command('sacct', '-j', identity, '-X', '--parsable2', '--noheader',
                      '--format=JobIDRaw,State,ExitCode,ElapsedRaw,Start,End')
        row = next(line.split('|') for line in raw.splitlines() if line.startswith(identity + '|'))
        status = row[1].split()[0].rstrip('+')
        if status not in TERMINAL:
            job['state'] = status
            continue
        allocation = (Path(state['training_root']) / f'final-{identity}.txt').read_text()
        assert fields(allocation)['JOB_GRES'] == f"gpu:V100:{job['world']}"
        elapsed = int(row[3])
        assert 0 <= elapsed <= job['wall_seconds']
        charge = elapsed * job['world'] / 8
        released = job['reserved_node_seconds']
        job.update(state=status, settled=True, exit_code=row[2], elapsed_seconds=elapsed,
                   charged_node_seconds=charge, reserved_node_seconds=0,
                   final_sacct_original=raw, allocation_final_original=allocation)
        state['charged_node_seconds'] += charge
        memory = json.loads((Path(state['training_root']) / f'memory-{identity}.json').read_text())
        state['request']['memory_mib'] = math.ceil(memory['peak_bytes'] * 1.25 / 1048576)
        save(root, state, dict(event='settled_final_Slurm', job_id=int(identity),
                              charged_node_seconds=charge, released_reservation_node_seconds=released))
        changed = True
    return changed


def snapshot(root, state, trigger):
    now = time.time()
    previous = state.get('resources')
    if now < state['next_resource_check_at'] or trigger in state['resource_triggers']:
        return previous, False
    state['resource_triggers'].append(trigger)
    state['last_resource_trigger'] = trigger
    state['next_resource_check_at'] = now + 630
    save(root, state, dict(event='resource_query_claim', trigger=trigger))
    jobs = command('scontrol', 'show', 'jobs', '-o')
    pending = [fields(line) for line in jobs.splitlines() if line.startswith('JobId=')]
    pending = [job for job in pending if job.get('JobState') == 'PENDING'
               and job['UserId'].split('(')[0] != 'zhuyouwei'
               and job.get('Reason') in ('Resources', 'Priority')]
    result = dict(queried_at=now, all_jobs_original=jobs, pending=pending,
                  own_queue=command('squeue', '-h', '-u', 'zhuyouwei', '-o', '%i|%T|%j'),
                  node=command('scontrol', 'show', 'node', 'gpu01', '-d', '-o'),
                  meminfo=Path('/proc/meminfo').read_text(),
                  disk=command('df', '-Pm', '/home/zhuyouwei'),
                  quota=command('quota', '-s'),
                  finished_at=time.time())
    state['resources'] = result
    state['next_resource_check_at'] = time.time() + 630
    save(root, state, dict(event='resource_query_completed', trigger=trigger, pending=pending))
    return result, True


def fullhash(root, state):
    repository = Path(state['repository'])
    assert command('git', 'rev-parse', 'HEAD', cwd=repository).strip() == state['source_commit']
    assert not command('git', 'status', '--porcelain=v1', cwd=repository).strip()
    rows = command('git', 'ls-files', '-s', '-z', cwd=repository).split('\0')
    records = []
    for row in filter(None, rows):
        metadata, name = row.split('\t', 1)
        raw = (repository / name).read_bytes()
        assert hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest() == metadata.split()[1]
        records.append(dict(path=str(repository / name), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
    runtime = list(Path(state['build']).glob('*.so')) + [Path(sys.executable), Path(__file__),
               root / 'stage12-chain.sbatch', Path(state['memory_probe'])]
    training = Path(state['training_root']) / 'training'
    runtime += [training / 'model.pt', training / 'baseline.json.gz'] + list(training.glob('rank-*.pt'))
    packages = Path(sys.prefix) / f'lib/python{sys.version_info.major}.{sys.version_info.minor}/site-packages'
    runtime += [path for path in packages.rglob('*') if path.is_file()
                and '__pycache__' not in path.parts and path.suffix != '.pyc']
    for path in sorted(set(runtime)):
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for data in iter(lambda: stream.read(1048576), b''):
                digest.update(data)
        records.append(dict(path=str(path), bytes=path.stat().st_size, sha256=digest.hexdigest()))
    result = dict(at=time.time(), source_commit=state['source_commit'], files=records)
    raw = (json.dumps(result, indent=2) + '\n').encode()
    path = root / f'fullhash-{time.time_ns()}.json'
    path.write_bytes(raw)
    return dict(at=result['at'], source_commit=state['source_commit'], files=len(records),
                path=str(path), sha256=hashlib.sha256(raw).hexdigest())


def enqueue(root, parent):
    with locked(root) as state:
        settle(root, state)
        pending = [identity for identity, job in state['jobs'].items()
                   if not job.get('settled') and identity != parent]
        if pending:
            assert len(pending) == 1
            return int(pending[0])
        if state.get('halted') or available_wall(state) < 60:
            save(root, state, dict(event='no_successor', reason='health_stop_or_budget_exhausted'))
            return None
        cached = state.get('resources')
        delay = max(0, state['next_resource_check_at'] - time.time()) if cached and time.time() - cached['finished_at'] > 300 else 0
    if delay:
        time.sleep(delay)
    with locked(root) as state:
        proof = fullhash(root, state)
    while True:
        with locked(root) as state:
            resources, _ = snapshot(root, state, 'enqueue-' + parent)
            delay = state['next_resource_check_at'] - time.time()
            if resources and time.time() - resources['finished_at'] <= 300:
                break
        time.sleep(max(1, delay))
    with locked(root) as state:
        settle(root, state)
        assert time.time() - proof['at'] <= 300
        assert time.time() - state['resources']['finished_at'] <= 300
        assert len([job for job in state['jobs'].values() if not job.get('settled')]) == 1
        own = [line.split('|')[0] for line in state['resources']['own_queue'].splitlines() if line]
        assert own == [parent]
        request = state['request'].copy()
        node = fields(state['resources']['node'])
        assert int(node['CPUTot']) >= request['cpus']
        assert int(node['RealMemory']) >= request['memory_mib']
        assert 'gpu:V100:' + str(request['world']) in node['Gres']
        wall = available_wall(state)
        if wall < 60:
            return None
        waiting = [job['JobId'] for job in state['resources']['pending']]
        dependency = 'afterany:' + ':'.join([parent] + waiting)
        argv = ['sbatch', '--hold', '--no-requeue', '--dependency=' + dependency,
                '--begin=now+11minutes', '--nodes=1', '--ntasks=1', '--partition=N56_128G_V100',
                '--nodelist=gpu01', f"--gres=gpu:V100:{request['world']}",
                f"--cpus-per-task={request['cpus']}", f"--mem={request['memory_mib']}M",
                f'--time={wall // 3600:02}:{wall % 3600 // 60:02}:00', '--job-name=spire-e4-stage12-chain',
                '--export=ALL,STAGE12_WORLD=' + str(request['world']) + ',STAGE12_PARENT=' + parent,
                '--chdir=' + state['repository'], '--output=' + str(root / 'chain-%j.log'),
                '--signal=B:USR1@240', str(root / 'stage12-chain.sbatch')]
        receipt = command(*argv)
        identity = re.fullmatch(r'Submitted batch job ([0-9]+)\n', receipt)[1]
        held = command('scontrol', 'show', 'job', identity, '-d', '-o')
        job = fields(held)
        assert job['UserId'].split('(')[0] == 'zhuyouwei'
        assert job['JobState'] == 'PENDING' and job['Priority'] == '0' and job['Reason'] == 'JobHeldUser'
        assert job['NumNodes'] in ('1', '1-1') and int(job['NumCPUs']) == request['cpus']
        assert job['Partition'] == 'N56_128G_V100' and job['ReqNodeList'] == 'gpu01'
        memory = re.fullmatch(r'([0-9.]+)([MGT]?)', job['MinMemoryNode'])
        assert float(memory[1]) * {'': 1, 'M': 1, 'G': 1024, 'T': 1048576}[memory[2]] == request['memory_mib']
        assert job['TresPerNode'] == f"gres:gpu:V100:{request['world']}"
        assert 'afterany:' + parent in job['Dependency']
        assert job['TimeLimit'] == f'{wall // 3600:02}:{wall % 3600 // 60:02}:00'
        amount = wall * request['world'] / 8
        state['jobs'][identity] = dict(job_id=int(identity), parent_job=int(parent), state='PENDING',
            world=request['world'], wall_seconds=wall, memory_mib=request['memory_mib'],
            cpus=request['cpus'], reserved_node_seconds=amount, reservation_original_node_seconds=amount,
            dependency=dependency, receipt_original=receipt, held_original=held,
            proof=proof, reserved_at=time.time(), settled=False)
        assert state['charged_node_seconds'] + reservation(state) <= state['limit_node_seconds']
        save(root, state, dict(event='reserve_after_real_held_receipt', job_id=int(identity),
                              reserved_node_seconds=amount, held_original=held, proof=proof))
        command('scontrol', 'release', identity)
        save(root, state, dict(event='successor_released', job_id=int(identity), parent_job=int(parent)))
        return int(identity)


def checkpoint_state(state):
    import torch
    path = Path(state['training_root']) / 'training/model.pt'
    checkpoint = torch.load(path, map_location='cpu', weights_only=False)
    rank = torch.load(path.parent / 'rank-0.pt', map_location='cpu', weights_only=False)
    assert tuple(rank['update']) == (checkpoint['state']['update'], checkpoint['state']['checkpoint_serial'])
    cursor = rank['cursor']
    steps = [int(item['step']) for item in checkpoint['optimizer']['state'].values() if 'step' in item]
    assert min(steps) == max(steps)
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
            pending = [identity for identity, job in state['jobs'].items()
                       if not job.get('settled') and job['state'] == 'PENDING']
            for identity in pending:
                parent = str(state['jobs'][identity]['parent_job'])
                dependency = 'afterany:' + ':'.join([parent] + [job['JobId'] for job in resources['pending']])
                command('scontrol', 'update', 'JobId=' + identity, 'Dependency=' + dependency)
                state['jobs'][identity]['dependency'] = dependency
            save(root, state, dict(event='yield_requested', pending=resources['pending']))
        return dict(state=state, queried=queried)


def run(root):
    identity = os.environ['SLURM_JOB_ID']
    parent = os.environ['STAGE12_PARENT']
    for _ in range(30):
        raw = command('sacct', '-j', parent, '-X', '--parsable2', '--noheader', '--format=JobIDRaw,State')
        status = next(line.split('|')[1].split()[0].rstrip('+') for line in raw.splitlines() if line.startswith(parent + '|'))
        if status in TERMINAL:
            break
        time.sleep(1)
    assert status in TERMINAL
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
    enqueue(root, identity)
    result = watch(root, 'start-' + identity)
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
