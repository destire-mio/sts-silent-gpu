"""Record or compare every observation byte against a separately built encoder."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct

import spire
from replay import open_trace, trace_paths


def check(paths, stream, compare):
    decisions = boundaries = 0
    for path in paths:
        with open_trace(path) as trace:
            header = json.loads(next(trace))
            game = spire.Game()
            if header['scenario'] is None:
                game.reset(header['seed'], header['playtime_at_least_800'])
            else:
                game.start_combat(header['seed'], header['scenario'])
            identity = str(path.resolve()).encode() + b'\0' + hashlib.sha256(path.read_bytes()).digest()
            if compare:
                assert stream.read(len(identity)) == identity, f'input changed: {path}'
            else:
                stream.write(identity)
            for line, text in enumerate(trace, 2):
                record = json.loads(text)
                observation = game.observation()
                assert observation[-1] == 0, (path, line, observation[-1])
                data = b''.join(array.tobytes() for array in observation[:-1])
                entities = sum(int(x).bit_count() for x in observation[5])
                candidates = sum(int(x).bit_count() for x in observation[6])
                data += struct.pack('<iiI', entities, candidates, observation[-1])
                if compare:
                    expected = stream.read(len(data))
                    assert expected == data, f'observation bytes differ: {path}:{line}'
                else:
                    stream.write(data)
                boundaries += 1
                if 'action' in record:
                    assert game.legal_actions() == record['legal'], (path, line, 'legal actions')
                    game.step(record['action'])
                    decisions += 1
                else:
                    assert 'terminal' in record
        print(f'PASS {path}', flush=True)
    if compare:
        assert stream.read(1) == b'', 'baseline contains additional records'
    print(f'observation bytes: {len(paths)} traces, {decisions} decisions, {boundaries} boundaries, zero differences', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('record', 'compare'))
    parser.add_argument('baseline', type=Path)
    parser.add_argument('paths', nargs='+', type=Path)
    parser.add_argument('--extra-deep', type=Path, required=True)
    args = parser.parse_args()
    paths = list(trace_paths(args.paths))
    for item in json.loads(args.extra_deep.read_text())['corpus']['extra_deep_traces']:
        path = Path(item['path'])
        assert hashlib.sha256(path.read_bytes()).hexdigest() == item['gzip_sha256'], path
        paths.append(path)
    with gzip.open(args.baseline, 'rb' if args.operation == 'compare' else 'wb', compresslevel=1) as stream:
        check(paths, stream, args.operation == 'compare')
