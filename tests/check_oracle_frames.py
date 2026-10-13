import gzip
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


def jobs(output):
    full_seeds = set(range(1, 32))
    recordings = []
    for path in sorted((ROOT / 'tests/traces').glob('*.jsonl.gz')):
        with gzip.open(path, 'rt') as stream:
            header = json.loads(next(stream))
        if header['scenario'] is None:
            full_seeds.add(header['seed'])
        else:
            recordings.append(dict(seed=header['seed'], scenario=header['scenario'],
                                   output=str(output / 'scenarios' / path.name[:-3])))
    recordings[:0] = [dict(seed=seed, scenario=None, output=str(output / 'full' / f'{seed}.jsonl'))
                      for seed in sorted(full_seeds)]
    recordings.extend(run_start_jobs(output / 'run-starts'))
    return recordings


def check(output):
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
    first = jobs(output / 'a')
    second = jobs(output / 'b')
    run_jobs(first)
    with (output / 'a/full/16.jsonl').open() as stream:
        snapshots = [json.loads(line)['snapshot'] for line in list(stream)[1:]]
    assert any(snapshot['screen']['type'] == 'GRID'
               and snapshot['objects'][snapshot['screen']['state']['ref']]['fields']['GridCardSelectScreen.forTransform']
               and snapshot['objects'][snapshot['screen']['state']['ref']]['fields']['GridCardSelectScreen.confirmScreenUp']
               for snapshot in snapshots)
    with (output / 'a/scenarios/Thievery-seed101.jsonl').open() as stream:
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
    output = Path(tempfile.mkdtemp(prefix='oracle-frames-'))
    print('Frame-check output:', output, flush=True)
    check(output)
