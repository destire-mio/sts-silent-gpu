import argparse
import collections
import fcntl
import gzip
import hashlib
import json
from pathlib import Path
import resource
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = json.loads((ROOT / 'inventory/seed-partitions.json').read_text())
STAGES = ('act2_entry', 'act3_entry', 'act3_boss', 'act4_entry', 'heart_start',
          'heart_turn2', 'heart_turn3', 'heart_turn5', 'act1_boss')
DECISION_LIMIT = 10_000_000
CPU_LIMIT = 6 * 3600


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def partition(seed):
    matches = [name for name in ('train', 'dev')
               if any(low <= seed <= high for low, high in PARTITIONS[name])]
    assert len(matches) == 1, f'Root seed is outside train/dev: {seed}'
    return matches[0]


def rows(path):
    with gzip.open(path, 'rt') if str(path).endswith('.gz') else Path(path).open() as stream:
        yield from (json.loads(line) for line in stream)


def write_gzip(path, records):
    with Path(path).open('xb') as raw, gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=0) as stream:
        for row in records:
            stream.write((json.dumps(row, separators=(',', ':')) + '\n').encode())


def cpu_seconds():
    own = resource.getrusage(resource.RUSAGE_SELF)
    children = resource.getrusage(resource.RUSAGE_CHILDREN)
    return own.ru_utime + own.ru_stime + children.ru_utime + children.ru_stime


def costs(output):
    path = output / 'ledger.jsonl'
    return list(rows(path)) if path.exists() else []


def account(output, item):
    with (output / 'ledger.jsonl').open('a') as stream:
        stream.write(json.dumps(item) + '\n')
    assert sum(row['decisions'] for row in costs(output)) <= DECISION_LIMIT
    assert sum(row['cpu_seconds'] for row in costs(output)) <= CPU_LIMIT
    assert sum(p.stat().st_size for p in output.rglob('*') if p.is_file()) < 5_000_000_000


def native(args, mode, input_path):
    prefix = args.output / args.name
    assert not prefix.with_suffix('.jsonl').exists()
    previous = costs(args.output)
    remaining = DECISION_LIMIT - sum(x['decisions'] for x in previous)
    cpu_remaining = CPU_LIMIT - sum(x['cpu_seconds'] for x in previous)
    assert 0 < args.decisions <= remaining and cpu_remaining > 0
    command = [str(args.build / 'spire_collect_roots'), mode, str(args.decisions),
               str(args.family_decisions), str(args.trials), str(cpu_remaining),
               str(prefix.with_suffix('.journal')), str(input_path)]
    config = dict(command=command, mode=mode, engine=subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        source_sha256=digest(ROOT / 'tools/collect_roots.cpp'), input_sha256=digest(input_path))
    prefix.with_suffix('.config.json').write_text(json.dumps(config, indent=2) + '\n')
    start = time.monotonic()
    with prefix.with_suffix('.jsonl').open('x') as out, prefix.with_suffix('.stderr').open('x') as err:
        process = subprocess.Popen(command, stdout=out, stderr=err)
        while process.poll() is None:
            time.sleep(1)
            if int(time.monotonic() - start) % 30 == 0:
                journal = prefix.with_suffix('.journal')
                if journal.exists():
                    with journal.open('rb') as stream:
                        count, seed, length = struct.unpack('QQQ', stream.read(24))
                    print(json.dumps(dict(name=args.name, decisions=count, seed=seed,
                                          prefix_length=length)), flush=True)
    records = list(rows(prefix.with_suffix('.jsonl')))
    if process.returncode:
        with prefix.with_suffix('.journal').open('rb') as stream:
            count, seed, length = struct.unpack('QQQ', stream.read(24))
            candidates = list(struct.unpack(f'{length}H', stream.read(length * 2)))
        prefix.with_suffix('.failure.json').write_text(json.dumps(dict(seed=seed, candidates=candidates)))
        account(args.output, dict(name=args.name, kind=mode, decisions=count,
                                  cpu_seconds=cpu_seconds(), failed=True))
        raise RuntimeError(f'Native collector exited {process.returncode}; preserved {prefix}.failure.json')
    result = records[-1]
    assert result['kind'] == 'cost'
    account(args.output, dict(name=args.name, kind=mode, decisions=result['decisions'],
                              cpu_seconds=cpu_seconds(), failed=False))
    print(json.dumps(result), flush=True)
    return records


def collect(args):
    args.output.mkdir(parents=True, exist_ok=True)
    seeds = args.output / f'{args.name}.seeds'
    with seeds.open('x') as stream:
        for i in range(max(args.train, args.dev * 2)):
            if i < args.train:
                seed = PARTITIONS['train'][0][0] + args.offset + i
                assert partition(seed) == 'train'
                stream.write(f'train {seed}\n')
            if i % 2 == 1 and i // 2 < args.dev:
                seed = PARTITIONS['dev'][0][0] + args.offset + i // 2
                assert partition(seed) == 'dev'
                stream.write(f'dev {seed}\n')
    records = native(args, 'collect', seeds)
    roots = [row for row in records if row['kind'] == 'root']
    write_gzip(args.output / f'{args.name}.roots.jsonl.gz', roots)
    print(json.dumps(dict(roots=len(roots), counts=dict(collections.Counter(
        (x['partition'] + '/' + x['stage']) for x in roots)))), flush=True)


def evaluate(args):
    roots = [row for file in args.inputs for row in rows(file)]
    seen = set()
    for row in roots:
        key = row['root']['seed'], row['stage']
        assert key not in seen
        seen.add(key)
        assert partition(key[0]) == row['partition']
    input_path = args.output / f'{args.name}.inputs'
    with input_path.open('x') as stream:
        for row in roots:
            root = row['root']
            stream.write(f"{row['partition']} {root['seed']} {STAGES.index(row['stage'])} "
                         f"{len(root['candidates'])} " + ' '.join(map(str, root['candidates'])) + '\n')
    records = native(args, 'evaluate', input_path)
    measured = {(x['seed'], x['stage']): x for x in records if x['kind'] == 'difficulty'}
    assert len(measured) == len(roots)
    for row in roots:
        row['difficulty'] = measured[row['root']['seed'], row['stage']]
    write_gzip(args.output / f'{args.name}.roots.jsonl.gz', roots)


def extend(args):
    families = collections.defaultdict(list)
    for file in args.inputs:
        for row in rows(file):
            assert partition(row['root']['seed']) == row['partition']
            families[row['root']['seed']].append(row)
    input_path = args.output / f'{args.name}.inputs'
    with input_path.open('x') as stream:
        for seed, found in families.items():
            row = max(found, key=lambda x: (x['floor'], len(x['root']['candidates'])))
            excluded = 0
            for item in found:
                excluded |= 1 << STAGES.index(item['stage'])
            prefix = row['root']['candidates']
            stream.write(f"{row['partition']} {seed} {excluded} {len(prefix)} " + ' '.join(map(str, prefix)) + '\n')
    records = native(args, 'extend', input_path)
    roots = [row for row in records if row['kind'] == 'root']
    write_gzip(args.output / f'{args.name}.roots.jsonl.gz', roots)
    print(json.dumps(dict(counts=dict(collections.Counter(x['partition'] + '/' + x['stage'] for x in roots)))))


def batch_check(args):
    import numpy as np
    import spire
    start = cpu_seconds()
    decisions = 0
    results = []
    roots = [row for file in args.inputs for row in rows(file)]
    assert sum(len(row['root']['candidates']) for row in roots) <= DECISION_LIMIT - sum(x['decisions'] for x in costs(args.output))
    for row in roots:
        root = row['root']
        assert partition(root['seed']) == row['partition']
        batch = spire.TrainingBatch(1, 1, 8192, [root['seed']])
        action = np.zeros(1, dtype=np.int32)
        for index in root['candidates']:
            assert not batch.done[0] and not batch.truncated[0] and not batch.faults[0]
            assert 0 <= index < batch.candidate_counts[0]
            action[0] = index
            batch.step(action)
            decisions += 1
        assert not batch.done[0] and not batch.truncated[0] and not batch.faults[0]
        assert batch.values[0, 0, 1] == row['hp']
        assert batch.values[0, 1, 1] == row['max_hp']
        assert batch.values[0, 1, 0] == row['floor']
        results.append(dict(seed=root['seed'], stage=row['stage'], decisions=len(root['candidates']), passed=True))
    account(args.output, dict(name=args.name, kind='training_batch_replay', decisions=decisions,
                              cpu_seconds=cpu_seconds() - start, failed=False))
    (args.output / f'{args.name}.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(dict(roots=len(results), decisions=decisions, passed=True)))


def validate(args):
    import spire
    sys.path.insert(0, str(ROOT / 'tests'))
    sys.path.insert(0, str(ROOT / 'oracle'))
    from record import run_jobs
    from replay import replay
    from check_oracle_frames import check_recordings
    start = cpu_seconds()
    roots = [row for file in args.inputs for row in rows(file)]
    groups = collections.defaultdict(list)
    for row in roots:
        groups[row['partition'], row['stage']].append(row)
    selected = []
    for key in sorted(groups):
        values = sorted(groups[key], key=lambda x: (x['hp'] / x['max_hp'], x['root']['seed']))
        for index in (0, len(values) // 2, len(values) - 1):
            if values[index] not in selected:
                selected.append(values[index])
    for row in roots:
        if len(selected) >= args.minimum_roots:
            break
        if row not in selected:
            selected.append(row)
    assert len(selected) >= args.minimum_roots
    destination = args.output / args.name
    destination.mkdir()
    jobs = []
    decisions = 0
    for index, row in enumerate(selected):
        root = row['root']
        assert partition(root['seed']) == row['partition']
        game = spire.reset(root['seed'])
        actions = []
        for candidate in root['candidates']:
            action = game.legal_actions()[candidate]
            actions.append(action)
            game.step(action)
            decisions += 1
        jobs.append(dict(seed=root['seed'], scenario={'actions': actions},
                         output=str(destination / f'{index}.jsonl.gz')))
    (destination / 'selection.json').write_text(json.dumps(selected, separators=(',', ':')) + '\n')
    (destination / 'jobs.json').write_text(json.dumps(jobs, separators=(',', ':')) + '\n')
    account(args.output, dict(name=args.name + '-prefix', kind='scenario_conversion', decisions=decisions,
                              cpu_seconds=cpu_seconds() - start, failed=False))
    start = cpu_seconds()
    recorded = False
    try:
        with Path('/private/tmp/spire-oracle.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            recording = run_jobs(jobs)
        recorded = True
    finally:
        account(args.output, dict(name=args.name + '-record', kind='original_recording', decisions=0,
                                  cpu_seconds=cpu_seconds() - start, failed=not recorded))
    start = cpu_seconds()
    decisions = 0
    results = []
    for row, job in zip(selected, jobs):
        result = replay(job['output'], spire)
        decisions += result.decisions
        results.append(dict(seed=job['seed'], partition=row['partition'], stage=row['stage'],
                            path=job['output'], sha256=digest(job['output']), decisions=result.decisions,
                            scopes={name: dict(passed=s.passed, matched_decisions=s.matched_decisions,
                                               terminal=s.terminal_matches, differences=s.differences)
                                    for name, s in result.scopes.items()}, stopped=result.stopped))
        print(json.dumps(results[-1]), flush=True)
    (destination / 'results.json').write_text(json.dumps(dict(recording=recording, results=results,
        idle_frames=13, byte_identical=None), indent=2) + '\n')
    account(args.output, dict(name=args.name, kind='original_validation', decisions=decisions,
                              cpu_seconds=cpu_seconds() - start,
                              failed=any(not s['passed'] for r in results for s in r['scopes'].values())))
    start = cpu_seconds()
    idle_passed = False
    try:
        with Path('/private/tmp/spire-oracle.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            delayed = destination / 'idle-frames'
            delayed.mkdir()
            check_recordings(delayed, jobs)
        idle_passed = True
    finally:
        account(args.output, dict(name=args.name + '-idle', kind='original_idle_frames', decisions=0,
                                  cpu_seconds=cpu_seconds() - start, failed=not idle_passed))
    (destination / 'results.json').write_text(json.dumps(dict(recording=recording, results=results,
        idle_frames=13, byte_identical=len(jobs)), indent=2) + '\n')
    assert all(s['passed'] for r in results for s in r['scopes'].values())


def distribution(values):
    return dict(sorted(collections.Counter(map(str, values)).items()))


def stage_summary(group):
    families = collections.Counter(x['root']['seed'] for x in group)
    max_share = max(families.values()) / len(group) if group else None
    return dict(roots=len(group), families=len(families),
        family_cap_satisfied=bool(group) and max_share <= 0.05, max_family_share=max_share,
        unique_decks=len({json.dumps(x['deck']) for x in group}),
        unique_relic_sets=len({json.dumps(x['relics']) for x in group}),
        unique_deck_relic_pairs=len({json.dumps([x['deck'], x['relics']]) for x in group}),
        hp=distribution(x['hp'] for x in group),
        deck_size=distribution(len(x['deck']) for x in group),
        relic_count=distribution(len(x['relics']) for x in group),
        potion_count=distribution(sum(p != 0 for p in x['potions']) for x in group),
        uniform_successes_out_of_16=distribution(x['difficulty']['uniform']['success'] for x in group),
        search_successes_out_of_4=distribution(x['difficulty']['search']['success'] for x in group),
        uniform_cutoffs=sum(x['difficulty']['uniform']['cutoff'] for x in group),
        search_cutoffs=sum(x['difficulty']['search']['cutoff'] for x in group))


def summarize(args):
    roots = [row for file in args.inputs for row in rows(file)]
    groups = collections.defaultdict(list)
    for row in roots:
        assert partition(row['root']['seed']) == row['partition']
        groups[row['partition'], row['stage']].append(row)
    counts = {}
    for part in ('train', 'dev'):
        counts[part] = {}
        for stage in STAGES:
            counts[part][stage] = stage_summary(groups[part, stage])
    audits = []
    for path in args.audits:
        report = json.loads(path.read_text())
        selected = json.loads((path.parent / 'selection.json').read_text())
        for row, root in zip(report['results'], selected):
            row['root_prefix_decisions'] = len(root['root']['candidates'])
        audits.append(dict(path=str(path), sha256=digest(path), results=report['results'],
                           idle_frames=report['idle_frames'], byte_identical=report['byte_identical']))
    parity_passed = len([r for audit in audits for r in audit['results']]) >= 20 and all(
        audit['byte_identical'] == len(audit['results']) and all(
            s['passed'] for row in audit['results'] for s in row['scopes'].values()) for audit in audits)
    family_counts = {part: len({x['root']['seed'] for x in roots if x['partition'] == part}) for part in ('train', 'dev')}
    family_targets = family_counts['train'] >= 256 and family_counts['dev'] >= 128
    covered = [stage for stage in STAGES if all(counts[part][stage]['family_cap_satisfied'] for part in ('train', 'dev'))]
    passed = parity_passed and family_targets and len(covered) >= 2
    exports = []
    for part in ('train', 'dev', 'sparse'):
        selected = [row for row in roots if (not counts[row['partition']][row['stage']]['family_cap_satisfied']
                    if part == 'sparse' else row['partition'] == part and counts[part][row['stage']]['family_cap_satisfied'])]
        path = args.output / f'{args.name}-{part}.jsonl.gz'
        write_gzip(path, selected)
        exports.append(dict(path=str(path), sha256=digest(path), bytes=path.stat().st_size, roots=len(selected),
                            released_for_curriculum=passed and part != 'sparse'))
    manifest = dict(version=1, engine_base='a7878b5b86c5700db0128678911d99dca4f54268',
        partition_sha256=digest(ROOT / 'inventory/seed-partitions.json'),
        root_schema='root.seed (signed int64), root.candidates (canonical legal-action indices); all other fields are metadata, never restore state',
        status='accepted' if passed else 'blocked_original_validation' if not parity_passed else 'insufficient_coverage',
        released_for_curriculum=passed,
        selection='First reachable boundary per seed and stage, including exploratory branches before choosing a turn. No filtering on continuation difficulty. Uniform sampling over families within eligible partition/stage. Stages with fewer than 20 families are diagnostic-only. No stage is released until original validation passes.',
        search_bias='Weighted card/reward/map policy and true-future search favor survival, HP, poison/scaling, and keys. Initial pilot/breadth/breadth3 retained one turn candidate; later searches retained up to two distinct-score turn candidates through a combat. Floor checkpoints retry failures. First Act2 roots were extended; the three Act3 families received 60000 decisions each. Act1 boss collection was added after scarce late-stage coverage. No search result is a training label.',
        goals=dict(zip(STAGES, ('act3_entry', 'act3_boss', 'act4_entry', 'heart_start', 'heart_win', 'heart_win', 'heart_win', 'heart_win', 'act2_entry'))),
        difficulty='16 uniform legal continuations capped at 2048 decisions each; 4 search RNG streams, 512 engine decisions each, up to two turn candidates and four sampled continuations per candidate. Search success is finding the target within this budget; returned deaths and live cutoffs are reported separately. The search returns its best finished branch when any finished branch exists, even if live branches remain at budget exhaustion. A zero search rate does not establish that a root is unwinnable. All trials keep the same game RNG world; only policy RNG varies. No difficulty filtering.',
        families=family_counts, family_targets_met=family_targets, family_cap_stages=covered,
        stages=counts, budget=dict(decision_limit=DECISION_LIMIT, cpu_seconds_limit=CPU_LIMIT,
                                 entries=costs(args.output)),
        validation=audits,
        search_configurations=[json.loads(p.read_text()) for p in sorted(args.output.glob('*.config.json'))],
        files=exports)
    args.manifest.write_text(json.dumps(manifest, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('command', choices=('collect', 'extend', 'evaluate', 'batch-check', 'validate', 'summarize'))
    parser.add_argument('--output', type=Path, default=Path('/private/tmp/e2-roots'))
    parser.add_argument('--build', type=Path, default=Path('/private/tmp/e2-build'))
    parser.add_argument('--name', required=True)
    parser.add_argument('--inputs', nargs='*', type=Path, default=[])
    parser.add_argument('--train', type=int, default=512)
    parser.add_argument('--dev', type=int, default=256)
    parser.add_argument('--offset', type=int, default=0)
    parser.add_argument('--decisions', type=int, default=6_000_000)
    parser.add_argument('--family-decisions', type=int, default=8000)
    parser.add_argument('--trials', type=int, default=6)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'inventory/e2-roots.json')
    parser.add_argument('--minimum-roots', type=int, default=20)
    parser.add_argument('--audits', nargs='*', type=Path, default=[])
    args = parser.parse_args()
    {'collect': collect, 'extend': extend, 'evaluate': evaluate, 'batch-check': batch_check,
     'validate': validate, 'summarize': summarize}[args.command](args)


if __name__ == '__main__':
    main()
