"""Record discard-cache regressions with ordinary and delayed original Soul flights."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'oracle'))
from record import run_jobs
from verify import compare
from check_oracle_frames import check_recordings

NAMES = ('GlassKnife-duplicate', 'GlassKnife-triple', 'GlassKnife-single', 'Rampage-duplicate')


def check(output):
    output.mkdir()
    instance = ROOT / 'oracle/instance'
    classes = output / 'classes'
    classes.mkdir()
    subprocess.run([str(instance / 'jre/bin/java'), '-jar',
                    '/Users/destire/Documents/Codex/2026-09-10/new-chat-2/work/native-protocol-source/ecj-4.8.jar',
                    '-1.8', '-proc:none', '-classpath', str(instance / 'ModTheSpire.jar'),
                    '-d', str(classes), str(ROOT / 'tests/OracleSoulFrames.java')], check=True)
    agent = output / 'soul-frames.jar'
    with zipfile.ZipFile(agent, 'w') as jar:
        jar.writestr('META-INF/MANIFEST.MF', 'Manifest-Version: 1.0\nPremain-Class: oracle.test.OracleSoulFrames\n\n')
        for path in classes.rglob('*.class'):
            jar.write(path, str(path.relative_to(classes)))
    jobs = [dict(seed=2000003426, scenario=json.loads((ROOT / 'tests/scenarios' / f'{name}.json').read_text()),
                 output=str(output / f'{name}-seed2000003426.jsonl.gz')) for name in NAMES]
    run_jobs(jobs)
    marker = output / 'active'
    os.environ['JAVA_TOOL_OPTIONS'] = f'-javaagent:{agent}={marker}'
    delayed = [dict(job, output=str(output / 'delayed' / Path(job['output']).name)) for job in jobs]
    run_jobs(delayed)
    del os.environ['JAVA_TOOL_OPTIONS']
    assert marker.read_text() == 'active\n'
    for original, changed in zip(jobs, delayed):
        compare(Path(original['output']), Path(changed['output']))
    idle = output / 'idle'
    idle.mkdir()
    check_recordings(idle, jobs)
    print('discard damage: 4 original recordings, 120-frame Soul delay and 13 idle frames byte-identical')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    check(parser.parse_args().output.resolve())
