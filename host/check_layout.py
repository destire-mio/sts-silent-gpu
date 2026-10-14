import argparse
import json
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--generated", type=Path, required=True)
    args = parser.parse_args()
    flags = [args.compiler, "-std=c++17", "-I", str(args.root), "-I", str(args.generated.parent)]
    with tempfile.TemporaryDirectory(prefix="spire-layout-") as temporary:
        source = Path(temporary) / "layout.cpp"
        source.write_text('#include "core/state.hpp"\n')
        ast = json.loads(subprocess.check_output(
            [*flags, "-fsyntax-only", "-Xclang", "-ast-dump=json", str(source)], text=True))
        records = {}

        def visit(node):
            if node.get("kind") == "NamespaceDecl" and node.get("name") == "spire":
                for child in node.get("inner", []):
                    if child.get("kind") == "CXXRecordDecl" and child.get("completeDefinition"):
                        records[child["name"]] = [field for field in child.get("inner", [])
                                                  if field.get("kind") in ("FieldDecl", "IndirectFieldDecl") and "name" in field]
            for child in node.get("inner", []):
                visit(child)

        visit(ast)
        lines = ['#include "core/state.hpp"', '#include <cstdio>', 'using namespace spire;',
                 'template<class T> struct Element { using Type = T; static constexpr int capacity = 1; };',
                 'template<class T, int N> struct Element<T[N]> { using Type = T; static constexpr int capacity = N; };']
        for container in ('List', 'Queue', 'Pool'):
            lines.append(f'template<class T, int N> struct Element<{container}<T,N>> {{ using Type = T; static constexpr int capacity = N; }};')
        lines.append('int main() {')
        for name, fields in records.items():
            lines.append(f'printf("{name}: %zu bytes\\n", sizeof({name}));')
            for field in fields:
                member = field["name"]
                if field.get("isBitfield"):
                    width = field["inner"][0]["value"]
                    lines.append(f'printf("  {member}: {width} bit(s)\\n");')
                else:
                    typename = f'decltype({name}::{member})'
                    lines.append(f'printf("  {member}: %zu (element %zu x %d)\\n", sizeof({typename}), sizeof(typename Element<{typename}>::Type), Element<{typename}>::capacity);')
        lines.append('}')
        source.write_text('\n'.join(lines))
        executable = Path(temporary) / "layout"
        subprocess.run([*flags, str(source), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
