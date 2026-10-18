"""Bounded V100 rooted collection, memory probes and DDP weak scaling."""
import argparse
from dataclasses import asdict
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time

import torch

from train.model import ModelConfig, Policy, record_lengths
from train.pipeline import Pipeline, PipelineConfig
from train.ppo import PPOConfig, update
from train.rollout import Collector, synchronize
from train.roots import read_roots
from train.sampling import random_words, root_words


CAP = 64
LANES = (512, 2048, 8192, 32768)
CHUNKS = (512, 2048)
MEASURED_VERSIONS = 3
MINIBATCH_PER_GPU = 2048
BUDGET_SECONDS = 1800
MIB = 1024**2


def emit(record):
    print(json.dumps(record, allow_nan=False), flush=True)


def roots_from(path):
    if str(path).endswith('.jsonl.gz'):
        with gzip.open(path, 'rt') as stream:
            return [json.loads(line)['root'] for line in stream]
    return read_roots(path)


def cycled_roots(roots, lanes, workers=1):
    return [roots[i % len(roots)] for i in range(lanes) for _ in range(workers)]


def storage_mib(lanes, profile):
    return math.ceil(lanes * CAP * profile['storage_bytes_per_decision'] / MIB)


def rates(decisions, collection_seconds, total_seconds, workers=1):
    return dict(collect_decisions_per_second=decisions / collection_seconds,
                per_gpu_collect_decisions_per_second=decisions / collection_seconds / workers,
                total_decisions_per_second=decisions / total_seconds,
                per_gpu_decisions_per_second=decisions / total_seconds / workers)


def parity(roots):
    torch.manual_seed(1729)
    cpu = torch.device('cpu')
    gpu = torch.device('cuda', 0)
    model = Policy(ModelConfig()).eval()
    state = model.state_dict()
    roots = roots[:16] + [dict(seed=1000000000 + i, candidates=[]) for i in range(4)]
    roots += [dict(seed=1000000020, candidates=[999])]
    keys = root_words(roots, cpu)
    attempts = torch.arange(len(roots)) + 2**33
    decisions = torch.arange(len(roots)) + 2**34
    torch.testing.assert_close(random_words(1729, keys, attempts, decisions),
        random_words(1729, keys.cuda(), attempts.cuda(), decisions.cuda()).cpu(), atol=0, rtol=0)
    expected = Collector(7, 32, 64, cpu, .95).collect(model, roots, 1729, range(len(roots)), policy_version=3)
    candidate = Policy(ModelConfig()).to(gpu).eval()
    candidate.load_state_dict(state)
    stream = torch.cuda.Stream()
    stream.wait_stream(torch.cuda.current_stream())
    with torch.cuda.stream(stream):
        actual = Collector(7, 32, 64, gpu, .95).collect(candidate, roots, 1729, range(len(roots)), policy_version=3)
    stream.synchronize()
    n = actual.count
    assert n == expected.count
    for a, b in zip(expected.episodes, actual.episodes):
        for name in ('root_key', 'attempt', 'candidates', 'records', 'outcome', 'fault', 'policy_version'):
            assert a[name] == b[name], (name, a, b)
    for name in ('offsets', 'actions', 'rewards', 'valid', 'complete', 'policy_versions'):
        length = n + (name == 'offsets')
        torch.testing.assert_close(getattr(expected, name)[:length], getattr(actual, name)[:length].cpu(), atol=0, rtol=0)
    length = int(expected.offsets[n])
    torch.testing.assert_close(expected.data[:length], actual.data[:length].cpu(), atol=0, rtol=0)
    errors = {}
    for name in ('log_probs', 'values', 'returns', 'advantages'):
        a, b = getattr(expected, name)[:n], getattr(actual, name)[:n].cpu()
        torch.testing.assert_close(a, b, atol=3e-5, rtol=3e-5)
        errors[name] = float((a-b).abs().max())
    permuted = Collector(3, 32, 64, gpu, .95).collect(candidate, list(reversed(roots)), 1729,
                                                   list(reversed(range(len(roots)))), policy_version=3)
    assert {e['attempt']: (e['candidates'], e['outcome']) for e in actual.episodes} == {
        e['attempt']: (e['candidates'], e['outcome']) for e in permuted.episodes}
    print(json.dumps(dict(check='CPU/GPU collector parity', decisions=n, compact_bytes=length,
                          exact_actions_outcomes_observations=True, max_absolute_error=errors)), flush=True)


def observation_profile(rollout):
    indices = torch.arange(rollout.count, device=rollout.data.device)
    entities, candidates = record_lengths(rollout.data, rollout.offsets, indices)
    lengths = rollout.offsets[1:rollout.count + 1] - rollout.offsets[:rollout.count]
    result = dict(decisions=rollout.count)
    for name, values in (('entities', entities), ('candidates', candidates), ('record_bytes', lengths)):
        values = values.double()
        result[name] = dict(mean=float(values.mean()), p95=float(torch.quantile(values, .95)), max=int(values.max()))
    result['storage_bytes_per_decision'] = math.ceil(result['record_bytes']['max'] * 1.25)
    return result


def calibrate(roots, output):
    device = torch.device('cuda', 0)
    torch.cuda.set_device(device)
    torch.manual_seed(1729)
    model = Policy(ModelConfig()).to(device)
    group = [roots[i * len(roots) // 128] for i in range(128)]
    collector = Collector(128, CAP, 128, device, .95, inference_chunk=128)
    rollout = collector.collect(model, group, 1729, range(128), policy_version=0)
    assert not any(e['outcome'] == 'fault' for e in rollout.episodes)
    result = dict(check='observation_profile', **observation_profile(rollout),
                  root_indices=[i * len(roots) // 128 for i in range(128)], decision_cap=CAP)
    output.write_text(json.dumps(result) + '\n')
    emit(result)


def probe(size, chunk, roots, profile, output):
    device = torch.device('cuda', 0)
    torch.cuda.set_device(device)
    torch.manual_seed(1729)
    model = Policy(ModelConfig()).to(device)
    config = PPOConfig(epochs=1, minibatch_size=MINIBATCH_PER_GPU, gae_lambda=.95)
    optimizer = torch.optim.Adam(model.parameters(), lr=config.learning_rate)
    rng = torch.Generator(device=device).manual_seed(1729)
    storage = storage_mib(size, profile)
    torch.cuda.empty_cache()
    synchronize(device)
    free_before, total = torch.cuda.mem_get_info()
    reserved_before = torch.cuda.memory_reserved()
    collector = Collector(size, CAP, storage, device, config.gae_lambda, inference_chunk=chunk)
    synchronize(device)
    free_after, _ = torch.cuda.mem_get_info()
    native = free_before - free_after
    warm = collector.collect(model, cycled_roots(roots, 128), 1729, range(128), policy_version=0)
    assert not any(e['outcome'] == 'fault' for e in warm.episodes)
    update(model, optimizer, warm, config, device, rng, policy_version=0)
    del warm
    torch.cuda.empty_cache()
    torch.cuda.reset_peak_memory_stats()
    synchronize(device)
    before = time.perf_counter()
    rollout = collector.collect(model, cycled_roots(roots, size), 1729, range(128, 128 + size), policy_version=1)
    collected = time.perf_counter()
    free_collected, _ = torch.cuda.mem_get_info()
    event_start, event_end = torch.cuda.Event(enable_timing=True), torch.cuda.Event(enable_timing=True)
    event_start.record()
    result = update(model, optimizer, rollout, config, device, rng, policy_version=1)
    event_end.record()
    synchronize(device)
    elapsed = time.perf_counter() - before
    free_updated, _ = torch.cuda.mem_get_info()
    assert not any(e['outcome'] == 'fault' for e in rollout.episodes)
    peak_allocated, peak_reserved = torch.cuda.max_memory_allocated(), torch.cuda.max_memory_reserved()
    record = dict(check='single_gpu', status='fit', lanes_per_gpu=size, inference_chunk=chunk,
        decisions=rollout.count, decision_cap=CAP, policy_version=1,
        **rates(rollout.count, collected - before, elapsed), collection_seconds=collected - before,
        update_seconds=elapsed - (collected - before), total_seconds=elapsed,
        cuda_event_seconds={**{k: v for k, v in rollout.timing.items() if k != 'collection_seconds'},
                            'update_seconds': event_start.elapsed_time(event_end) / 1000},
        memory=dict(native_batch_bytes=native, free_before_batch_bytes=free_before,
            free_after_batch_bytes=free_after, free_after_collect_bytes=free_collected,
            free_after_update_bytes=free_updated, device_total_bytes=total,
            torch_peak_allocated_bytes=peak_allocated, torch_peak_reserved_bytes=peak_reserved,
            native_plus_torch_peak_allocated_bytes=native + peak_allocated,
            device_peak_used_estimate_bytes=total - free_before - reserved_before + native + peak_reserved),
        storage_mib=storage, compact_bytes=int(rollout.offsets[rollout.count]),
        observations=observation_profile(rollout), ppo=result, model=asdict(model.config), fp32=True)
    output.write_text(json.dumps(record) + '\n')
    emit(record)


def exact(workers, output, roots):
    group = [roots[i * len(roots) // 32] for i in range(32)]
    model_config, ppo = ModelConfig(), PPOConfig(epochs=1, minibatch_size=256)
    config = PipelineConfig(workers=workers, batch_size=32, decision_cap=16, storage_mib=16,
                            inference_chunk=16)
    manifest = dict(model=asdict(model_config), ppo=asdict(ppo), engine_commit='day-one', observation_version=2)
    output.mkdir()
    with Pipeline(model_config, ppo, config, 1729, output, manifest) as pipeline:
        pipeline.initialize()
        rollout = pipeline.collect(group, 1729, range(len(group)))
        assert not any(e['outcome'] == 'fault' for e in rollout.episodes)
        assert all(e['policy_version'] == 0 for e in rollout.episodes)
        result = [dict(attempt=e['attempt'], candidates=e['candidates'], outcome=e['outcome']) for e in rollout.episodes]
        assert pipeline.update(rollout)['published_version'] == 1
    (output / 'result.json').write_text(json.dumps(result) + '\n')
    emit(dict(check='exact', workers=workers, decisions=rollout.count))


def scaling(workers, size, chunk, storage, output, roots):
    model_config = ModelConfig()
    ppo = PPOConfig(epochs=1, minibatch_size=MINIBATCH_PER_GPU * workers, gae_lambda=.95)
    config = PipelineConfig(workers=workers, batch_size=size, inference_chunk=chunk,
                            decision_cap=CAP, storage_mib=storage)
    manifest = dict(model=asdict(model_config), ppo=asdict(ppo), engine_commit='day-one', observation_version=2)
    group = cycled_roots(roots, size, workers)
    total = len(group)
    output.mkdir()
    records = []
    with Pipeline(model_config, ppo, config, 1729, output, manifest) as pipeline:
        pipeline.initialize()
        for version in range(MEASURED_VERSIONS + 1):
            emit(dict(check='weak_scaling_version', workers=workers, version=version, phase='start', warmup=version == 0))
            before = time.perf_counter()
            rollout = pipeline.collect(group, 1729, range(version * total, (version + 1) * total))
            collected = time.perf_counter()
            assert all(e['policy_version'] == version for e in rollout.episodes)
            assert not any(e['outcome'] == 'fault' for e in rollout.episodes)
            result = pipeline.update(rollout)
            elapsed = time.perf_counter() - before
            assert result['published_version'] == version + 1
            record = dict(version=version, warmup=version == 0, decisions=rollout.count,
                **rates(rollout.count, collected - before, elapsed, workers),
                collection_seconds=collected - before, update_seconds=elapsed - (collected - before),
                total_seconds=elapsed, cuda_collect_event_seconds_sum={k: v for k, v in rollout.timing.items()
                                                                       if k != 'collection_seconds'}, ppo=result)
            records.append(record)
            emit(dict(check='weak_scaling_version', workers=workers, phase='done', **record))
    measured = records[1:]
    summary = dict(check='weak_scaling', workers=workers, lanes_per_gpu=size, inference_chunk=chunk,
        decision_cap=CAP, global_minibatch_size=ppo.minibatch_size, measured_versions=MEASURED_VERSIONS,
        **rates(sum(r['decisions'] for r in measured), sum(r['collection_seconds'] for r in measured),
                sum(r['total_seconds'] for r in measured), workers), updates=records)
    (output / 'result.json').write_text(json.dumps(summary) + '\n')
    emit({k: v for k, v in summary.items() if k != 'updates'})


def run_stage(name, command, output, deadline, seconds, env):
    started = time.monotonic()
    stop = min(deadline, started + seconds)
    log_path = output / f'{name}.log'
    emit(dict(stage=name, phase='start', limit_seconds=max(0, stop - started), log=str(log_path)))
    with log_path.open('w') as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=env, start_new_session=True)
        while True:
            remaining = stop - time.monotonic()
            if remaining <= 0:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                status = 'timeout'
                break
            try:
                code = process.wait(timeout=min(15, remaining))
            except subprocess.TimeoutExpired:
                emit(dict(stage=name, phase='running', elapsed_seconds=time.monotonic() - started))
                continue
            status = 'ok' if code == 0 else 'failed'
            break
    result = dict(stage=name, status=status, seconds=time.monotonic() - started, log=str(log_path))
    emit(result)
    return result


def require(result):
    assert result['status'] == 'ok', result


def select_probe(rows):
    fitted = [row for row in rows if row['status'] == 'fit']
    assert fitted, 'No rooted collect+update probe completed within the memory/time limits'
    return max(fitted, key=lambda row: row['total_decisions_per_second'])


def scaling_table(reports):
    baseline = reports[0]['per_gpu_decisions_per_second']
    return [dict(**{k: v for k, v in r.items() if k != 'updates'},
                 efficiency=r['per_gpu_decisions_per_second'] / baseline) for r in reports]


def suite(roots, build, output):
    started = time.monotonic()
    deadline = math.inf
    output.mkdir()
    (output / 'tmp').mkdir()
    repo = Path(__file__).resolve().parents[1]
    env = dict(os.environ, TMPDIR=str(output / 'tmp'), PYTHONPATH=f'{build}:{repo}',
               CUDA_VISIBLE_DEVICES='0,1,2,3,4,5,6,7')
    gpu_env = dict(env, CUDA_VISIBLE_DEVICES='0')
    rows, reports, stages = [], [], []
    summary = dict(check='final_summary', status='incomplete', roots_sha256=hashlib.sha256(roots.read_bytes()).hexdigest(),
                   budget_seconds=BUDGET_SECONDS, single_gpu=rows, weak_scaling=[], stages=stages)
    def stage(name, command, seconds, environment=env, until=deadline):
        result = run_stage(name, command, output, until, seconds, environment)
        stages.append(result)
        return result
    def command(check, *options):
        return [sys.executable, '-m', 'tools.gpu_train_check', check, '--roots', str(roots), *map(str, options)]
    try:
        preflight = 'import torch; assert torch.cuda.device_count() == 8; assert "sm_70" in torch.cuda.get_arch_list(); assert all(torch.cuda.get_device_capability(i) == (7,0) for i in range(8)); print(torch.__version__, torch.version.cuda, [torch.cuda.get_device_properties(i) for i in range(8)])'
        require(stage('preflight', [sys.executable, '-c', preflight], 30))
        require(stage('hardware', ['nvidia-smi'], 30))
        require(stage('configure', ['cmake', '-S', str(repo), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release',
            f'-DCMAKE_CXX_COMPILER={shutil.which("clang++")}', f'-DCMAKE_CUDA_COMPILER={shutil.which("clang++")}',
            f'-DCMAKE_CUDA_COMPILER_LAUNCHER={sys.executable};{repo / "tools/cuda_compile.py"};compile',
            f'-DCUDAToolkit_ROOT={Path(shutil.which("nvcc")).resolve().parents[1]}', '-DCMAKE_CUDA_ARCHITECTURES=70',
            f'-DPython_EXECUTABLE={sys.executable}', '-DBUILD_TESTING=ON'], 600))
        require(stage('build', ['cmake', '--build', str(build), '--parallel', '2'], 7200))
        require(stage('compile_times', [sys.executable, str(repo / 'tools/cuda_compile.py'), 'report', str(build)], 30))
        require(stage('ctest', ['ctest', '--test-dir', str(build), '--output-on-failure', '--no-tests=error', '-j1'],
                      3600))
        deadline = time.monotonic() + BUDGET_SECONDS
        require(stage('parity', command('parity'), 300))
        profile_path = output / 'profile.json'
        require(stage('calibrate', command('calibrate', '--output', profile_path), 180, gpu_env))
        reference = None
        for world in (1, 2, 8):
            target = output / f'exact-{world}'
            require(stage(f'exact-{world}', command('exact', '--workers', world, '--output', target), 180))
            actual = json.loads((target / 'result.json').read_text())
            if reference is None:
                reference = actual
            assert actual == reference, f'Version-0 actions/outcomes differ at {world} GPUs'
        summary['exact_version_zero_worlds'] = [1, 2, 8]
        sweep_deadline = min(deadline - 720, time.monotonic() + 720)
        for size in LANES:
            for chunk in CHUNKS:
                row = dict(lanes_per_gpu=size, inference_chunk=chunk)
                if time.monotonic() >= sweep_deadline:
                    rows.append(dict(row, status='time_budget'))
                    continue
                target = output / f'probe-{size}-{chunk}.json'
                result = stage(f'probe-{size}-{chunk}', command('probe', '--size', size, '--inference-chunk', chunk,
                    '--profile', profile_path, '--output', target), 150, gpu_env, sweep_deadline)
                if result['status'] == 'ok':
                    row = json.loads(target.read_text())
                elif result['status'] == 'timeout':
                    row.update(status='timeout')
                else:
                    assert 'out of memory' in Path(result['log']).read_text().lower(), result
                    row.update(status='oom')
                rows.append(row)
                emit(row)
        best = select_probe(rows)
        summary['selected'] = {key: best[key] for key in ('lanes_per_gpu', 'inference_chunk', 'storage_mib')}
        summary['largest_fitting_lanes'] = max(r['lanes_per_gpu'] for r in rows if r['status'] == 'fit')
        emit(dict(check='selected', **summary['selected'], largest_fitting_lanes=summary['largest_fitting_lanes']))
        for i, world in enumerate((1, 2, 8)):
            target = output / f'ddp-{world}'
            require(stage(f'ddp-{world}', command('scaling', '--workers', world, '--size', best['lanes_per_gpu'],
                '--inference-chunk', best['inference_chunk'], '--storage-mib', best['storage_mib'], '--output', target),
                max(0, (deadline - time.monotonic()) / (3 - i))))
            reports.append(json.loads((target / 'result.json').read_text()))
            summary['weak_scaling'] = scaling_table(reports)
            emit(summary['weak_scaling'][-1])
        summary['status'] = 'passed'
    finally:
        summary['elapsed_seconds'] = time.monotonic() - started
        (output / 'training-throughput.json').write_text(json.dumps(summary, indent=2) + '\n')
        emit(summary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('check', choices=('suite', 'parity', 'calibrate', 'probe', 'exact', 'scaling'))
    parser.add_argument('--roots', type=Path, required=True)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--profile', type=Path)
    parser.add_argument('--size', type=int)
    parser.add_argument('--inference-chunk', type=int)
    parser.add_argument('--storage-mib', type=int)
    parser.add_argument('--workers', type=int)
    args = parser.parse_args()
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    if args.check == 'suite':
        suite(args.roots.resolve(), args.build.resolve(), args.output.resolve())
    else:
        roots = roots_from(args.roots)
        if args.check == 'parity':
            parity(roots)
        elif args.check == 'calibrate':
            calibrate(roots, args.output)
        elif args.check == 'probe':
            probe(args.size, args.inference_chunk, roots, json.loads(args.profile.read_text()), args.output)
        elif args.check == 'exact':
            exact(args.workers, args.output, roots)
        else:
            scaling(args.workers, args.size, args.inference_chunk, args.storage_mib, args.output, roots)


if __name__ == '__main__':
    main()
