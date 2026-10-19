import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'oracle'))
from record import run_jobs
from verify import compare
from check_oracle_run_starts import jobs as run_start_jobs
from replay import open_trace, trace_paths


def jobs(output, paths=()):
    full_seeds = set(range(1, 32))
    recordings = []
    seen = set()
    fixtures = sorted((ROOT / 'tests/traces').glob('*.jsonl.gz'))
    for index, path in enumerate(fixtures + list(trace_paths(paths))):
        with open_trace(path) as stream:
            header = json.loads(next(stream))
        if header['scenario'] is None:
            full_seeds.add(header['seed'])
        else:
            key = json.dumps([header['seed'], header['scenario']], sort_keys=True)
            if index >= len(fixtures) and key in seen:
                continue
            seen.add(key)
            destination = output / 'scenarios' / path.name if index < len(fixtures) else output / 'extra' / f'{index}.jsonl.gz'
            recordings.append(dict(seed=header['seed'], scenario=header['scenario'],
                                   output=str(destination)))
    recordings[:0] = [dict(seed=seed, scenario=None, output=str(output / 'full' / f'{seed}.jsonl.gz'))
                      for seed in sorted(full_seeds)]
    for job in run_start_jobs(output / 'run-starts'):
        key = json.dumps([job['seed'], job['scenario']], sort_keys=True)
        if key not in seen:
            seen.add(key)
            recordings.append(job)
    return recordings


def idle_agent(output):
    instance = ROOT / 'oracle/instance'
    classes = output / 'classes'
    classes.mkdir()
    subprocess.run([str(instance / 'jre/bin/java'), '-jar',
                    '/Users/destire/Documents/Codex/2026-09-10/new-chat-2/work/native-protocol-source/ecj-4.8.jar',
                    '-1.8', '-proc:none', '-classpath', str(instance / 'ModTheSpire.jar'),
                    '-d', str(classes), str(ROOT / 'tests/OracleIdleFrames.java')], check=True)
    agent = output / 'idle-frames.jar'
    with zipfile.ZipFile(agent, 'w') as jar:
        jar.writestr('META-INF/MANIFEST.MF', 'Manifest-Version: 1.0\nPremain-Class: oracle.test.OracleIdleFrames\n\n')
        for path in classes.rglob('*.class'):
            jar.write(path, str(path.relative_to(classes)))
    return agent


def check_recordings(output, recordings):
    agent = idle_agent(output)
    delayed = [dict(job, output=str(output / 'delayed' / f'{index}.jsonl.gz'))
               for index, job in enumerate(recordings)]
    marker = output / 'probe-active'
    os.environ['JAVA_TOOL_OPTIONS'] = '-javaagent:' + str(agent) + '=' + str(marker)
    run_jobs(delayed)
    del os.environ['JAVA_TOOL_OPTIONS']
    assert marker.read_text() == 'active\n'
    for original, extra in zip(recordings, delayed):
        compare(Path(original['output']), Path(extra['output']))
    print(f'13 extra frames per observation: {len(recordings)} recordings byte-identical', flush=True)


def check(output, paths=(), inputs=()):
    agent = idle_agent(output)
    first = jobs(output / 'a', paths)
    second = jobs(output / 'b', paths)
    first.extend(inputs)
    second.extend(dict(job, output=str(output / 'b' / 'fresh' / f'{index}.jsonl.gz'))
                  for index, job in enumerate(inputs))
    run_jobs(first)
    with open_trace(output / 'a/full/16.jsonl.gz') as stream:
        snapshots = [json.loads(line)['snapshot'] for line in list(stream)[1:]]
    assert any(snapshot['screen']['type'] == 'GRID'
               and snapshot['objects'][snapshot['screen']['state']['ref']]['fields']['GridCardSelectScreen.forTransform']
               and snapshot['objects'][snapshot['screen']['state']['ref']]['fields']['GridCardSelectScreen.confirmScreenUp']
               for snapshot in snapshots)
    with open_trace(output / 'a/scenarios/Thievery-seed101.jsonl.gz') as stream:
        terminal = json.loads(list(stream)[-1])['snapshot']
    escaped = [obj['fields'] for obj in terminal['objects']
               if obj['fields'].get('AbstractMonster.escaped')]
    assert escaped and all(monster['AbstractCreature.drawX'] is None for monster in escaped)
    marker = output / 'probe-active'
    os.environ['JAVA_TOOL_OPTIONS'] = '-javaagent:' + str(agent) + '=' + str(marker)
    run_jobs(second)
    assert marker.read_text() == 'active\n'
    for a, b in zip(first, second):
        compare(Path(a['output']), Path(b['output']))
    full = sum(job['scenario'] is None for job in first)
    starts = sum(job['scenario'] is not None and 'start' in job['scenario'] for job in first)
    print(f'13 extra frames per observation: {full} full runs, {len(first) - full - starts} combat scenarios '
          f'and {starts} run-start scenarios byte-identical')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Compare original recordings with 13 extra idle frames before every observation')
    parser.add_argument('traces', nargs='*', type=Path)
    parser.add_argument('--jobs', type=Path, help='Record these seed/scenario/output jobs in the same idle-frame check')
    args = parser.parse_args()
    output = Path(tempfile.mkdtemp(prefix='oracle-frames-'))
    print('Frame-check output:', output, flush=True)
    check(output, args.traces, json.loads(args.jobs.read_text()) if args.jobs else ())
