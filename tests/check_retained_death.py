import argparse
import fcntl
import json
from pathlib import Path
import sys

import spire

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'oracle'))
from record import run_jobs
from check_oracle_frames import check_recordings
from replay import replay


def check(output):
    output.mkdir()
    job = json.loads((ROOT / 'tests/retained_death_cache.json').read_text())
    job['output'] = str(output / 'baseline.jsonl.gz')
    with Path('/private/tmp/spire-oracle.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        run_jobs([job])
        result = replay(job['output'], spire)
        scopes = {name: dict(passed=scope.passed, terminal=scope.terminal_matches,
                            matched_decisions=scope.matched_decisions, differences=scope.differences)
                  for name, scope in result.scopes.items()}
        (output / 'replay.json').write_text(json.dumps(dict(decisions=result.decisions,
            stopped=result.stopped, scopes=scopes), indent=2) + '\n')
        print(json.dumps(scopes), flush=True)
        delayed = output / 'idle-frames'
        delayed.mkdir()
        check_recordings(delayed, [job])
    assert result.stopped is None and all(scope.passed and scope.terminal_matches
                                         for scope in result.scopes.values())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Record retained-card death caches and check both replay scopes and 13 idle frames')
    parser.add_argument('--output', required=True, type=Path)
    check(parser.parse_args().output.resolve())
