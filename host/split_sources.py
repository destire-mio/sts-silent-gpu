"""Lower the engine's restricted C++ rule inputs to declarations and split definitions."""
import json
import re
from collections import defaultdict

LITERALS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)


def masked(text):
    return LITERALS.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text)


def close(text, start, left='{', right='}'):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == left:
            depth += 1
        elif text[i] == right:
            depth -= 1
            if depth == 0:
                return i
    raise ValueError(f'unclosed {left} at {start}')


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def split(text, origin):
    clean = masked(text)
    scopes = []
    for m in re.finditer(r'\b(namespace|struct|class)\s+([\w:]+)[^;{}]*\{', clean):
        start = m.end() - 1
        scopes.append((start, close(clean, start), m[1], m[2]))
    def enclosing(pos):
        return [(kind, name) for start, end, kind, name in scopes if start < pos < end]
    edits, definitions, functions = [], [], []
    for m in re.finditer(r'(?m)^[ \t]*(?:\[\[noreturn\]\] )?(?:static )?SPIRE_FN\b[^\n]*', clean):
        start = m.start()
        paren = clean.index('(', start)
        end_params = close(clean, paren, '(', ')')
        body = end_params + 1
        while clean[body].isspace(): body += 1
        if clean.startswith('const', body):
            body += 5
            while clean[body].isspace(): body += 1
        if clean[body] != '{': continue
        end = close(clean, body) + 1
        functions.append((start, end))
        signature = text[start:body].strip()
        before = clean[:start].rstrip()
        template = before.endswith('>') and before[before.rfind('\n') + 1:].lstrip().startswith('template')
        scope = enclosing(start)
        namespace = '::'.join(n for k, n in scope if k == 'namespace')
        classes = '::'.join(n for k, n in scope if k != 'namespace')
        name = re.search(r'(\w+)\s*$', clean[start:paren])[1]
        if name == 'data' and classes and 'constexpr' in signature and '::units::' in namespace:
            datatype = re.search(r'constexpr (\w+) data', signature)[1]
            declaration = text[start:body].replace(f'constexpr {datatype}', f'const {datatype}&').rstrip() + ';'
            edits.append((start, end, declaration))
            definition = f'namespace {namespace} {{\nstruct {classes}Data : {classes} {{\n'
            definition += f'static SPIRE_FN SPIRE_INLINE constexpr {datatype} value() ' + text[body:end] + '\n};\n'
            definition += f'SPIRE_CONSTANT const {datatype} {classes}_data = {classes}Data::value();\n'
            definition += f'SPIRE_FN const {datatype}& {classes}::data() {{ return {classes}_data; }}\n}}\n'
            definitions.append((name, definition))
            class_end = next(b for a,b,k,n in scopes if k == 'struct' and n == classes)
            edits.append((class_end + 2, class_end + 2, f'\nextern SPIRE_CONSTANT const {datatype} {classes}_data;'))
            continue
        if 'constexpr' in signature or template:
            edits.append((start, body, text[start:body].replace('SPIRE_FN', 'SPIRE_FN SPIRE_INLINE')))
            continue
        declaration = re.sub(r'\binline\s+', '', text[start:body]).rstrip() + ';'
        edits.append((start, end, declaration))
        signature = re.sub(r'\b(static|inline)\s+', '', signature)
        signature = re.sub(r'\s*=\s*(?:\{\}|[^,)]+)', '', signature)
        if classes:
            signature = re.sub(r'\b' + name + r'\s*\(', classes + '::' + name + '(', signature, count=1)
        line = text.count('\n', 0, start) + 1
        definition = f'namespace {namespace} {{\n#line {line} "{origin}"\n' + signature + text[body:end] + '\n}\n'
        definitions.append((name, definition))
    for m in re.finditer(r'(?m)^constexpr\s+([\w:]+(?:<[\w:]+>)?)\s+(\w+)((?:\[[^\]]*\])*)\s*(?:=\s*)?\{', clean):
        start, body = m.start(), m.end()-1
        if any(a <= start < b for a, b in functions): continue
        end = close(clean, body)+1
        while clean[end].isspace(): end += 1
        assert clean[end] == ';'
        end += 1
        dims = m[3]
        if dims.startswith('[]'):
            depth, count, has_item = 0, 0, False
            for c in clean[body+1:end-2]:
                if c in '{([': depth += 1
                if c in '})]': depth -= 1
                if c == ',' and depth == 0: count += 1; has_item = False
                elif not c.isspace(): has_item = True
            dims = f'[{count + int(has_item)}]' + dims[2:]
        storage = 'SPIRE_STORAGE' if m[1].startswith('ReadOnly<') else 'SPIRE_CONSTANT const'
        edits.append((start, end, f'extern {storage} {m[1]} {m[2]}{dims};'))
        namespace = '::'.join(n for k,n in enclosing(start) if k == 'namespace')
        definition = re.sub(r'^constexpr', storage, text[start:end])
        definitions.append((m[2], f'namespace {namespace} {{\n{definition}\n}}\n'))
    for start, end, replacement in sorted(edits, reverse=True):
        text = text[:start] + replacement + text[end:]
    return text, definitions


def emit(root, output, generated):
    groups = defaultdict(list)
    inputs = sorted(root.glob('core/**/*.inc')) + sorted(root.glob('obs/*.inc'))
    for path in inputs:
        relative = path.relative_to(root).with_suffix('.hpp')
        header, definitions = split(path.read_text(), root / relative)
        write(output / relative, header)
        area = path.parent.name if path.parent != root / 'core' else 'core_' + path.stem
        if path.parent == root / 'obs': area = 'obs_' + path.stem
        for index, (name, definition) in enumerate(definitions):
            group = f'{area}_{index // 12:02d}' if area.startswith(('core_', 'obs_')) else f'{area}_{(ord(path.stem[0]) - ord("a")) // 4:02d}'
            groups[group].append(definition)
    for name, content in generated.items():
        header, definitions = split(content, output / name)
        write(output / name, header)
        for index, (function, definition) in enumerate(definitions):
            prefix = 'base' if name == 'bases.hpp' else function.split('_')[0]
            groups[f'dispatch_{prefix}_{index // 24:02d}'].append(definition)
    sources = []
    for name, definitions in sorted(groups.items()):
        source = f'sources/{name}.cpp'
        includes = '#include "core/engine.hpp"\n'
        if name.startswith('obs_'):
            includes += '#include "obs/encode.hpp"\n#include "obs/storage.hpp"\n'
        write(output / source, includes + '\n'.join(definitions))
        write(output / f'sources/{name}.cu', f'#include "{name}.cpp"\n')
        sources.append(source)
    for path in (output / 'sources').iterdir():
        if str(path.relative_to(output).with_suffix('.cpp')) not in sources:
            path.unlink()
    write(output / 'sources.cmake', 'set(SPIRE_ENGINE_SOURCES\n' + ''.join(f'    "${{SPIRE_GENERATED_DIR}}/{s}"\n' for s in sources) + ')\n')
    write(output / 'sources.json', json.dumps(sources, indent=2) + '\n')
