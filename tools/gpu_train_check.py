"""V100 collector parity, bounded end-to-end probes, and single-node DDP scaling."""
import argparse
from dataclasses import asdict
import gzip
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import torch

from train.model import ModelConfig, Policy
from train.pipeline import Pipeline, PipelineConfig
from train.ppo import PPOConfig, update
from train.rollout import Collector, synchronize
from train.roots import read_roots
from train.sampling import random_words, root_words


def roots_from(path):
    if str(path).endswith('.jsonl.gz'):
        with gzip.open(path, 'rt') as stream:
            return [json.loads(line)['root'] for line in stream]
    return read_roots(path)


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


def probe(size, output):
    device = torch.device('cuda', 0)
    torch.cuda.set_device(device)
    torch.manual_seed(1729)
    model = Policy(ModelConfig()).to(device)
    config = PPOConfig(epochs=1, minibatch_size=256, gae_lambda=.95)
    optimizer = torch.optim.Adam(model.parameters(), lr=config.learning_rate)
    cap = 4
    storage = (size * cap * 10180 + 1024**2 - 1) // 1024**2
    collector = Collector(size, cap, storage, device, config.gae_lambda)
    warm = collector.collect(model, [dict(seed=1000000000, candidates=[])], 1729, [0], policy_version=0)
    update(model, optimizer, warm, config, device, torch.Generator(device=device).manual_seed(1729), policy_version=0)
    del warm
    torch.manual_seed(1729)
    model = Policy(ModelConfig()).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=config.learning_rate)
    roots = [dict(seed=1000000000 + i, candidates=[]) for i in range(size)]
    torch.cuda.reset_peak_memory_stats()
    synchronize(device)
    before = time.perf_counter()
    rollout = collector.collect(model, roots, 1729, range(size), policy_version=0)
    collected = time.perf_counter()
    result = update(model, optimizer, rollout, config, device, torch.Generator(device=device).manual_seed(1729), policy_version=0)
    synchronize(device)
    elapsed = time.perf_counter() - before
    free, total = torch.cuda.mem_get_info()
    record = dict(check='single GPU collect+update', size=size, decisions=rollout.count,
        decisions_per_second=rollout.count/elapsed, collection_seconds=collected-before,
        update_seconds=elapsed-(collected-before), total_seconds=elapsed, timing=rollout.timing,
        torch_peak_allocated_bytes=torch.cuda.max_memory_allocated(), device_free_bytes=free,
        device_total_bytes=total, storage_mib=storage, ppo=result, model=asdict(model.config), fp32=True)
    output.write_text(json.dumps(record) + '\n')
    print(json.dumps(record), flush=True)


def scaling(workers, output, roots):
    total, cap = 512, 16
    model_config = ModelConfig()
    ppo = PPOConfig(epochs=1, minibatch_size=256, gae_lambda=.95)
    config = PipelineConfig(workers=workers, batch_size=(total + workers - 1)//workers,
                            decision_cap=cap, storage_mib=256)
    manifest = dict(model=asdict(model_config), ppo=asdict(ppo), engine_commit='day-one', observation_version=2)
    roots = [roots[i % len(roots)] for i in range(total)]
    output.mkdir()
    records = []
    with Pipeline(model_config, ppo, config, 1729, output, manifest) as pipeline:
        pipeline.initialize()
        for version in range(2):
            before = time.perf_counter()
            rollout = pipeline.collect(roots, 1729, range(version * total, (version + 1)*total))
            collected = time.perf_counter()
            assert all(e['policy_version'] == version for e in rollout.episodes)
            assert not any(e['outcome'] == 'fault' for e in rollout.episodes)
            result = pipeline.update(rollout)
            elapsed = time.perf_counter() - before
            assert result['published_version'] == version + 1
            records.append(dict(version=version, decisions=rollout.count, decisions_per_second=rollout.count/elapsed,
                collection_seconds=collected-before, update_seconds=elapsed-(collected-before),
                total_seconds=elapsed, ppo=result, episodes=rollout.episodes))
    summary = dict(check='DDP', workers=workers, updates=records,
                   decisions_per_second=sum(r['decisions'] for r in records)/sum(r['total_seconds'] for r in records))
    (output/'result.json').write_text(json.dumps(summary)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k != 'updates'}), flush=True)


def suite(roots, output):
    subprocess.run([sys.executable, "-m", "tools.gpu_train_check", "parity", "--roots", str(roots)], check=True)
    records = []
    for size in (32, 128, 512, 2048, 8192, 32768, 65536, 131072, 262144):
        target = output / f'probe-{size}.json'
        with (output/f'probe-{size}.log').open('w') as log:
            process = subprocess.run([sys.executable, '-m', 'tools.gpu_train_check', 'probe', '--size', str(size),
                                      '--output', str(target)], stdout=log, stderr=subprocess.STDOUT,
                                     env=dict(os.environ, CUDA_VISIBLE_DEVICES='0'))
        if process.returncode:
            error = (output/f'probe-{size}.log').read_text()
            assert 'out of memory' in error.lower(), error
            record = dict(check='memory_limit', size=size, log=str(output/f'probe-{size}.log'))
            records.append(record)
            print(json.dumps(record), flush=True)
            break
        record = json.loads(target.read_text())
        records.append(record)
        print(json.dumps(record), flush=True)
    else:
        raise AssertionError('Sweep did not reach the memory boundary')
    reports = []
    for world in (1, 2, 8):
        target = output/f'ddp-{world}'
        subprocess.run([sys.executable, '-m', 'tools.gpu_train_check', 'scaling', '--workers', str(world),
                        '--roots', str(roots), '--output', str(target)], check=True)
        report = json.loads((target/'result.json').read_text())
        reports.append(report)
        first = reports[0]['updates'][0]['episodes']
        actual = report['updates'][0]['episodes']
        assert [(e['attempt'], e['candidates'], e['outcome']) for e in first] == [
                (e['attempt'], e['candidates'], e['outcome']) for e in actual]
        record = dict(check='scaling', workers=world, decisions_per_second=report['decisions_per_second'],
                      speedup=report['decisions_per_second']/reports[0]['decisions_per_second'])
        records.append(record)
        print(json.dumps(record), flush=True)
    (output/'training-throughput.json').write_text(json.dumps(records, indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('check', choices=('suite', 'parity', 'probe', 'scaling'))
    parser.add_argument('--roots', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--size', type=int)
    parser.add_argument('--workers', type=int)
    args = parser.parse_args()
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    if args.check == 'suite':
        suite(args.roots, args.output)
    elif args.check == 'parity':
        parity(roots_from(args.roots))
    elif args.check == 'probe':
        probe(args.size, args.output)
    else:
        scaling(args.workers, args.output, roots_from(args.roots))


if __name__ == '__main__':
    main()
