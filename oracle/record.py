import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parent


def run_jobs(jobs):
    instance = ROOT / 'instance'
    for job in jobs:
        Path(job['output']).parent.mkdir(parents=True, exist_ok=True)
    job_dir = Path(tempfile.mkdtemp(prefix='oracle-job-'))
    for name in ('desktop-1.0.jar', 'jre', 'ModTheSpire.jar', 'launcher.jar', 'mods'):
        (job_dir / name).symlink_to(instance / name)
    (job_dir / 'tmp').mkdir()
    (job_dir / 'home').mkdir()
    (job_dir / 'jobs.json').write_text(json.dumps(jobs))
    command = [str(instance / 'jre/bin/java'), '-Xmx1G', '-Djava.awt.headless=true',
               '-Duser.home=' + str(job_dir / 'home'), '-Djava.io.tmpdir=' + str(job_dir / 'tmp'),
               '-Doracle.jobs=' + str(job_dir / 'jobs.json'), '-javaagent:launcher.jar',
               '-cp', 'launcher.jar:ModTheSpire.jar',
               'com.evacipated.cardcrawl.modthespire.Loader', '--mods',
               'oracle', '--skip-intro']
    print('JVM log:', job_dir / 'game.log', flush=True)
    with (job_dir / 'game.log').open('w') as log:
        process = subprocess.run(command, cwd=job_dir, stdout=log, stderr=subprocess.STDOUT)
    audit = (job_dir / 'bytecode-audit.txt').read_text().splitlines()
    assert audit[0] == 'loaded 2306 original 2306', audit[0]
    changed = {line.split()[0].removeprefix('com.megacrit.cardcrawl.') for line in audit[1:]}
    expected = json.loads((ROOT.parent / 'inventory/runtime-patches.json').read_text())
    assert changed == expected.keys(), (changed - expected.keys(), expected.keys() - changed)
    process.check_returncode()
    result = json.loads((job_dir / 'result.json').read_text())
    print(json.dumps(result), flush=True)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Record Silent A20 games in one JVM')
    parser.add_argument('--seed', type=int, required=True)
    parser.add_argument('--count', type=int, default=1)
    parser.add_argument('--scenario', type=Path)
    parser.add_argument('--workers', type=int, choices=(1, 2, 3), default=1)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    scenario = json.loads(args.scenario.read_text()) if args.scenario else None
    jobs = [dict(seed=args.seed + i, scenario=scenario,
                 output=str(args.output.resolve() / f'{args.seed + i}.jsonl')) for i in range(args.count)]
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        list(pool.map(run_jobs, [jobs[i::args.workers] for i in range(min(args.workers, args.count))]))
