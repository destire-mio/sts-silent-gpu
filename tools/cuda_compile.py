"""CMake CUDA launcher and GNU time per-object report."""
import re
import subprocess
import sys
from pathlib import Path


def report(build):
    print('| CUDA object | Wall seconds | Peak RSS KiB | Exit |')
    print('|---|---:|---:|---:|')
    paths = sorted(build.rglob('*.o.time'))
    assert paths, f'No CUDA compiler timings in {build}'
    for path in paths:
        text = path.read_text()
        elapsed = re.search(r'Elapsed \(wall clock\) time .*: ([\d:.]+)', text)[1]
        seconds = 0.0
        for part in elapsed.split(':'): seconds = seconds * 60 + float(part)
        rss = int(re.search(r'Maximum resident set size \(kbytes\): (\d+)', text)[1])
        status = int(re.search(r'Exit status: (\d+)', text)[1])
        print(f'| {path.relative_to(build)} | {seconds:.2f} | {rss} | {status} |')


def main():
    operation, *args = sys.argv[1:]
    if operation == 'report':
        report(Path(args[0]))
        return 0
    assert operation == 'compile'
    output = Path(args[args.index('-o') + 1])
    return subprocess.run(['/usr/bin/time', '-v', '-o', str(output) + '.time', *args]).returncode


if __name__ == '__main__':
    raise SystemExit(main())
