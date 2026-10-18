"""Compile the production device units and check their PTX link interface without a toolkit."""
import argparse
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import subprocess
import tempfile


FUNCTION = re.compile(r'(?m)^(?:(\.visible|\.weak|\.extern)\s+)?\.(func|entry)\s+(?:\([^)]*\)\s*)?([\w$]+)\s*\([^)]*\)\s*(?:\.noreturn\s*)?([;{])')
GLOBAL = re.compile(r'(?m)^[ \t]*(?:(\.visible|\.weak|\.extern)\s+)?\.(global|const|shared)\s+[^;\n=]*?\b([\w$]+)(?:\[\d*\])?\s*(?:=|;)')


def inspect(ptx):
    ptx = re.sub(r"//[^\n]*", lambda m: " " * len(m[0]), ptx)
    functions = list(FUNCTION.finditer(ptx))
    definitions, external, local = {}, set(), set()
    sizes = []
    for m in functions:
        visibility, _, name, ending = m.groups()
        if ending == ';':
            external.add(name)
            continue
        end = ptx.index('\n}', m.end()) + 2
        sizes.append((ptx[m.start():end].count('\n') + 1, name))
        if visibility is None: local.add(name)
        else:
            assert name not in definitions, name
            definitions[name] = 'function'
    for m in GLOBAL.finditer(ptx):
        visibility, _, name = m.groups()
        if visibility == '.extern': external.add(name)
        elif visibility is None: local.add(name)
        else:
            assert name not in definitions, name
            definitions[name] = 'global'
    identifiers = set(re.findall(r'(?<![\w$])[_A-Za-z$][\w$]*', ptx))
    references = {name for name in identifiers if name.startswith('_Z')} | external
    references.update(re.findall(r'\bcall(?:\.uni)?\s+(?:\([^)]*\),\s*)?([\w$]+)\s*,', ptx))
    references -= local
    references = {name for name in references if not re.search(r'_param_\d+$', name)}
    return definitions, references, len(sizes), max(sizes, default=(0, '-'))


def closure(units):
    owners = defaultdict(list)
    for name, (definitions, _, _, _) in units.items():
        for symbol in definitions: owners[symbol].append(name)
    errors = [f'duplicate {symbol}: {", ".join(names)}' for symbol, names in owners.items() if len(names) != 1]
    for name, (_, references, _, _) in units.items():
        errors += [f'{name}: undefined {symbol}' for symbol in sorted(references - owners.keys())]
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--generated', type=Path, required=True)
    parser.add_argument('--compiler', required=True)
    args = parser.parse_args()
    sources = [args.generated / p for p in json.loads((args.generated / 'sources.json').read_text())]
    sources += sorted((args.root / 'cuda').glob('*.cu'))
    sources += sorted((args.root / 'tests').glob('test_cuda_*.cu'))
    with tempfile.TemporaryDirectory(prefix='spire-ptx-') as temp:
        def compile_unit(source):
            output = Path(temp) / (source.stem + '.ptx')
            command = [args.compiler, '-std=c++17', '-x', 'cuda', '--cuda-device-only', '--cuda-gpu-arch=sm_70',
                       '-nocudainc', '-nocudalib', '-Xclang', '-target-feature', '-Xclang', '+ptx70',
                       '-fgpu-rdc', '-O3', '-ffp-contract=off', '-fno-slp-vectorize', '-mllvm', '-inline-threshold=5', '-S',
                       '-include', str(args.root / 'tests/cuda_stub/cuda_runtime.h'),
                       '-I', str(args.root / 'tests/cuda_stub'), '-I', str(args.generated),
                       '-DSPIRE_DEEP_ACTIONS="' + str(args.root / 'tests/deep_benchmark.actions') + '"',
                       '-I', str(args.root), '-I', str(args.generated.parent), str(source), '-o', str(output)]
            result = subprocess.run(command, capture_output=True, text=True)
            assert result.returncode == 0, result.stderr
            ptx = output.read_text()
            return source.stem, ptx.count('\n'), inspect(ptx)
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(compile_unit, sources))
    units = {}
    print('| Unit | PTX lines | Functions | Largest function (lines) |')
    print('|---|---:|---:|---|')
    for name, lines, symbols in results:
        units[name] = symbols
        count, largest = symbols[2:]
        demangled = subprocess.check_output(['c++filt', '-n', largest[1]], text=True).strip()
        print(f'| {name} | {lines} | {count} | {demangled} ({largest[0]}) |')
    errors = closure(units)
    errors += [f'{name}: {lines} PTX lines exceeds 60000' for name, lines, _ in results if lines > 60000]
    for error in errors: print(error)
    if errors: return 1
    print(f'Device PTX link closure passed: {len(results)} units, {sum(n for _, n, _ in results)} lines, no duplicate or undefined symbols')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
