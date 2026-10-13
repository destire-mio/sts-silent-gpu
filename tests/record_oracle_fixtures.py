import argparse
import gzip
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'oracle'))
from record import run_jobs
from verify import check_trace


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Regenerate all committed fixtures from their recorded headers')
    parser.add_argument('--output', required=True, type=Path)
    output = parser.parse_args().output.resolve()
    jobs = []
    for path in sorted((ROOT / 'traces').glob('*.jsonl.gz')):
        with gzip.open(path, 'rt') as stream:
            header = json.loads(next(stream))
        jobs.append(dict(seed=header['seed'], scenario=header['scenario'],
                         output=str(output / path.name.removesuffix('.gz'))))
    run_jobs(jobs)
    for job in jobs:
        raw = Path(job['output'])
        check_trace(raw)
        (ROOT / 'traces' / (raw.name + '.gz')).write_bytes(gzip.compress(raw.read_bytes(), mtime=0))
