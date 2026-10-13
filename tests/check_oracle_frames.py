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
from verify import compare, suite


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
    first = [suite(output / 'a')[i] for i in (15, 20, 23, 216)]
    second = [suite(output / 'b')[i] for i in (15, 20, 23, 216)]
    run_jobs(first)
    with Path(first[0]['output']).open() as stream:
        snapshots = [json.loads(line)['snapshot'] for line in list(stream)[1:]]
    assert any(snapshot['screen']['type'] == 'GRID'
               and snapshot['objects'][snapshot['screen']['state']['ref']]['fields']['GridCardSelectScreen.forTransform']
               and snapshot['objects'][snapshot['screen']['state']['ref']]['fields']['GridCardSelectScreen.confirmScreenUp']
               for snapshot in snapshots)
    marker = output / 'probe-active'
    os.environ['JAVA_TOOL_OPTIONS'] = '-javaagent:' + str(agent) + '=' + str(marker)
    run_jobs(second)
    assert marker.read_text() == 'active\n'
    for a, b in zip(first, second):
        compare(Path(a['output']), Path(b['output']))
    print('13 extra frames per decision: 1 full run (including transform preview) and 3 scenarios byte-identical')


if __name__ == '__main__':
    output = Path(tempfile.mkdtemp(prefix='oracle-frames-'))
    print('Frame-check output:', output, flush=True)
    check(output)
