import shutil
import argparse
import hashlib
from pathlib import Path
import subprocess
import zipfile
import json

ROOT = Path(__file__).resolve().parent
REFERENCE = Path('/Users/destire/Documents/Codex/2026-09-10/new-chat-2/work')
GAME = Path('/Users/destire/Library/Application Support/Steam/steamapps/common/SlayTheSpire/SlayTheSpire.app/Contents/Resources')
GAME_HASH = 'dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a'


def build():
    instance = ROOT / 'instance'
    instance.mkdir(exist_ok=True)
    sources = {
        'desktop-1.0.jar': GAME / 'desktop-1.0.jar',
        'jre': GAME / 'jre',
        'ModTheSpire.jar': REFERENCE / 'java-a20-j5/ModTheSpire.jar',
    }
    assert hashlib.sha256(sources['desktop-1.0.jar'].read_bytes()).hexdigest() == GAME_HASH
    for name, source in sources.items():
        target = instance / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.is_symlink():
            target.symlink_to(source)
        assert target.resolve() == source.resolve()
    classes = instance / 'classes'
    if classes.exists():
        shutil.rmtree(classes)
    classes.mkdir()
    (instance / 'mods').mkdir(exist_ok=True)
    cp = ':'.join(str(instance / x) for x in sources if x.endswith('.jar'))
    java = str(instance / 'jre/bin/java')
    subprocess.run([java, '-jar', str(REFERENCE / 'native-protocol-source/ecj-4.8.jar'),
                    '-1.8', '-proc:none', '-nowarn', '-classpath', cp, '-d', str(classes),
                    *map(str, sorted((ROOT / 'java').glob('*.java')))], check=True)
    subprocess.run([java, '-cp', str(classes) + ':' + str(instance / 'ModTheSpire.jar'),
                    'BuildLogicLauncher', str(instance / 'ModTheSpire.jar'),
                    str(instance / 'launcher.jar.tmp')], check=True)
    with zipfile.ZipFile(instance / 'launcher.jar.tmp', 'a') as jar:
        jar.writestr('META-INF/MANIFEST.MF', 'Manifest-Version: 1.0\nPremain-Class: oracle.audit.BytecodeAudit\n\n')
        for file in sorted((classes / 'oracle/audit').glob('*.class')):
            jar.write(file, str(file.relative_to(classes)))
    (instance / 'launcher.jar.tmp').replace(instance / 'launcher.jar')
    subprocess.run([java, '-cp', str(classes) + ':' + cp, 'oracle.FieldSchema',
                    str(instance / 'desktop-1.0.jar'), str(ROOT.parent / 'inventory/snapshot-fields.json')], check=True)
    with zipfile.ZipFile(instance / 'mods/Oracle.jar.tmp', 'w', zipfile.ZIP_DEFLATED) as jar:
        jar.write(ROOT.parent / 'inventory/snapshot-fields.json', 'snapshot-fields.json')
        jar.writestr('ModTheSpire.json', json.dumps({
            'modid': 'oracle', 'name': 'Silent Oracle', 'author_list': ['Oracle'],
            'version': '0.1.0', 'sts_version': '12-18-2022', 'mts_version': '3.30.3',
            'dependencies': [],
            'description': 'In-process original-game trace recorder',
        }))
        for file in sorted((classes / 'oracle').glob('*.class')):
            jar.write(file, str(file.relative_to(classes)))
    (instance / 'mods/Oracle.jar.tmp').replace(instance / 'mods/Oracle.jar')
    print('Built oracle/instance/mods/Oracle.jar')


if __name__ == '__main__':
    argparse.ArgumentParser(description='Build the local original-game recorder').parse_args()
    build()
