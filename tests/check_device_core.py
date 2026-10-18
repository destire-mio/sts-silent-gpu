import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


ALLOWED_HEADERS = {"cstdint", "cstddef"}
EXTENSIONS = {".h", ".hpp", ".cuh", ".cpp", ".cu", ".def", ".inc"}
CATALOG_EXTENSIONS = {".def", ".inc"}
FORBIDDEN = re.compile(
    r"\b(new|delete|throw|try|catch|virtual|dynamic_cast|typeid|thread_local|mutable)\b"
    r"|\b(?:__builtin_)?(malloc|calloc|realloc|free|alloca|setjmp|longjmp)\s*\("
)
COMMENTS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
FUNCTION_KINDS = {"FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl", "CXXConversionDecl"}


def position(location):
    return location.get("expansionLoc", location)


def check(root, compiler, generated=None, extra_roots=()):
    root = root.resolve()
    roots = [root]
    roots.extend(path.resolve() for path in extra_roots)
    if generated is not None:
        roots.append(generated.resolve())
    include_dirs = [directory.parent for directory in roots]
    if generated is not None:
        include_dirs.insert(0, generated.resolve())
    sources = {path.resolve(): path.read_text() for directory in roots
               for path in sorted(directory.rglob("*")) if path.suffix in EXTENSIONS}
    errors = []
    for path, source in sources.items():
        for match in re.finditer(r'^\s*#\s*(include|import)\s+([^\n]+)', source, re.M):
            literal = re.match(r'([<"])([^>"\n]+)[>"]', match.group(2))
            if match.group(1) != "include" or literal is None:
                errors.append(f"{path}: nonliteral or forbidden include {match.group().strip()}")
                continue
            delimiter, header = literal.groups()
            if delimiter == "<":
                permitted = header in ALLOWED_HEADERS
            else:
                candidates = [path.parent / header] + [directory / header for directory in include_dirs]
                permitted = any(candidate.resolve() in sources for candidate in candidates)
            if not permitted:
                errors.append(f"{path}:{source.count(chr(10), 0, match.start()) + 1}: forbidden header {header}")
        stripped = COMMENTS.sub(lambda m: "\n" * m.group().count("\n"), source)
        for match in FORBIDDEN.finditer(stripped):
            errors.append(f"{path}: forbidden construct {match.group().strip()}")
    if errors:
        return errors
    with tempfile.TemporaryDirectory(prefix="spire-device-check-") as temporary:
        unit = Path(temporary) / "core.cpp"
        unit.write_text("".join(f'#include "{path}"\n' for path in sources if path.suffix not in CATALOG_EXTENSIONS and path.suffix != ".cu"))
        result = subprocess.run(
            [compiler, "-std=c++17", "-fno-exceptions", "-fno-rtti", "-pedantic-errors", "-x", "c++", "-fsyntax-only", "-Xclang", "-ast-dump=json",
             *[argument for directory in include_dirs for argument in ("-I", str(directory))], str(unit)],
            capture_output=True, text=True,
        )
    if result.returncode:
        return [result.stderr.rstrip()]
    ast = json.loads(result.stdout)
    functions = {}
    edges = {}
    previous = {}
    current_file = None

    def visit(node, function=None):
        nonlocal current_file
        loc = position(node.get("loc", {}))
        begin = position(node.get("range", {}).get("begin", {}))
        explicit_file = loc.get("file", begin.get("file"))
        if explicit_file:
            current_file = Path(explicit_file).resolve()
        path = current_file
        own = path in sources
        kind = node.get("kind")
        implicit = node.get("isImplicit", False)
        if kind in FUNCTION_KINDS and not implicit:
            function = node["id"]
            if own:
                name = node.get("name", "<unnamed>")
                functions[function] = (path, name)
                edges.setdefault(function, set())
                if node.get("previousDecl"):
                    previous[node["previousDecl"]] = function
                source = sources[path]
                start = begin.get("offset", 0)
                end = loc.get("offset", start)
                boundary = max(source.rfind(";", 0, start), source.rfind("}", 0, start), source.rfind("{", 0, start))
                prefix = source[boundary + 1:end]
                prefix = re.sub(r"^\s*#.*$", "", prefix, flags=re.M)
                prefix = COMMENTS.sub("", prefix)
                if not re.search(r"\bSPIRE_FN\b", prefix):
                    errors.append(f"{path}: {name} lacks SPIRE_FN")
        if own and not implicit:
            if kind == "VarDecl" and (function is None or node.get("storageClass") == "static"):
                typename = re.sub(r"\[[^]]*\]", "", node.get("type", {}).get("qualType", "")).strip()
                constant = (typename.startswith("const ") and "*" not in typename) or typename.endswith(" const")
                if not node.get("constexpr", False) and not constant:
                    errors.append(f"{path}: mutable global or static {node.get('name')}")
            if kind in {"VarDecl", "FieldDecl", "ParmVarDecl", "TypedefDecl", "TypeAliasDecl"}:
                typename = node.get("type", {}).get("qualType", "")
                if re.search(r"\([^)]*\*[^)]*\)\s*\(", typename):
                    errors.append(f"{path}: function pointer {node.get('name')}")
            if kind in {"CXXNewExpr", "CXXDeleteExpr", "CXXThrowExpr", "CXXTryStmt", "CXXDynamicCastExpr", "CXXTypeidExpr", "LambdaExpr"}:
                errors.append(f"{path}: forbidden construct {kind}")
        if function in edges and kind == "DeclRefExpr":
            declaration = node.get("referencedDecl", {})
            if declaration.get("kind") in FUNCTION_KINDS:
                edges[function].add(declaration["id"])
        if function in edges and kind == "MemberExpr" and node.get("referencedMemberDecl"):
            edges[function].add(node["referencedMemberDecl"])
        for child in node.get("inner", []):
            visit(child, function)

    visit(ast)
    checked = set()

    def cycles(function, active):
        if function in active:
            path, name = functions[function]
            errors.append(f"{path}: recursion through {name}")
            return
        if function in checked:
            return
        checked.add(function)
        for target in edges.get(function, ()):
            while target in previous:
                target = previous[target]
            if target in functions:
                cycles(target, active | {function})

    for function in functions:
        cycles(function, set())
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--generated", type=Path, required=True)
    parser.add_argument("--extra-root", type=Path, action="append", default=[])
    args = parser.parse_args()
    errors = check(args.root, args.compiler, args.generated, args.extra_root)
    if not errors:
        with tempfile.TemporaryDirectory(prefix="spire-cuda-check-") as temporary:
            unit = Path(temporary) / "batch.cu"
            unit.write_text('''#define __CUDACC__ 1
#define __device__ __attribute__((device))
#define __host__ __attribute__((host))
#define __global__ __attribute__((global))
#define __shared__ __attribute__((shared))
struct dim3 { unsigned x, y, z; };
extern __device__ const dim3 threadIdx, blockIdx, blockDim;
#include "cuda/batch.cu"
#include "obs/encode.hpp"
#include "obs/storage.hpp"
__global__ void encode_observation(const spire::Game* games, spire::obs::Observation* output) {
    spire::ActionSet actions;
    spire::obs::History history;
    spire::obs::encode(games[threadIdx.x], history, output[threadIdx.x].view(), actions);
}
__global__ void compact_observation(spire::obs::Observation* input, unsigned char* storage, spire::obs::Observation* output) {
    auto* record = storage + threadIdx.x * sizeof(spire::obs::Observation);
    spire::obs::store(input[threadIdx.x].view(), record);
    spire::obs::pad(record, output[threadIdx.x].view());
}
''')
            result = subprocess.run(
                [args.compiler, "-std=c++17", "-x", "cuda", "--cuda-device-only", "-nocudainc",
                 "-nocudalib", "-O1", "-S", "-emit-llvm", "-o", str(Path(temporary) / "batch.ll"), "-I", str(args.root.resolve().parent),
                 "-I", str(args.generated.resolve()), "-I", str(args.generated.resolve().parent), str(unit)], capture_output=True, text=True)
            if result.returncode:
                errors.append(result.stderr.rstrip())
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    print("Device C++ and CUDA device LLVM generation checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
