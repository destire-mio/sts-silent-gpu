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
    parser = argparse.ArgumentParser(description='Regenerate the seven original-game fixtures')
    parser.add_argument('--output', required=True, type=Path)
    output = parser.parse_args().output.resolve()
    jobs = [dict(seed=1, scenario=None, output=str(output / 'full-run-1.jsonl'))]
    for i, path in enumerate(sorted((ROOT / 'scenarios').glob('*.json'))):
        jobs.append(dict(seed=101 + i, scenario=json.loads(path.read_text()),
                         output=str(output / (path.stem + '.jsonl'))))
    assert len(jobs) == 7
    run_jobs(jobs)
    for job in jobs:
        raw = Path(job['output'])
        check_trace(raw)
        (ROOT / 'traces' / (raw.name + '.gz')).write_bytes(gzip.compress(raw.read_bytes(), mtime=0))
