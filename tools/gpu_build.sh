#!/usr/bin/env bash
set -euo pipefail
build=$(realpath -m "${1:?usage: tools/gpu_build.sh BUILD_DIR}")
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo"

export TMPDIR="${TMPDIR:-$build/tmp}" OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
mkdir -p "$TMPDIR"
python=${SPIRE_PYTHON:?set SPIRE_PYTHON to the server venv Python}
clang=$(command -v clang++)
nvcc=$(command -v nvcc)
cuda=$(dirname "$(dirname "$(realpath "$nvcc")")")
"$clang" --version
"$nvcc" --version
cmake -S . -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$clang" \
    -DCMAKE_CUDA_COMPILER="$clang" -DCUDAToolkit_ROOT="$cuda" \
    -DCMAKE_CUDA_COMPILER_LAUNCHER="$python;$repo/tools/cuda_compile.py;compile" \
    -DCMAKE_CUDA_ARCHITECTURES=70 -DSPIRE_CUDA_INLINE_THRESHOLD=5 \
    -DPython_EXECUTABLE="$python" -DBUILD_TESTING=OFF
cmake --build "$build" --target spire_gpu --parallel 2
"$python" tools/cuda_compile.py report "$build"
