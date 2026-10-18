#!/usr/bin/env bash
# One-off comparison: device inlining thresholds 5 and 250.
# Builds sequentially, checks CPU/GPU parity for each, then measures engine speed on one GPU.
set -euo pipefail
work=$(realpath -m "${1:?usage: tools/inline_check.sh WORK_DIR}")
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo"
test ! -e "$work"
mkdir -p "$work/tmp"
export TMPDIR="$work/tmp" CUDA_VISIBLE_DEVICES=0
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
python=${SPIRE_PYTHON:?set SPIRE_PYTHON to the server venv Python}
clang=$(command -v clang++)
cuda=$(dirname "$(dirname "$(realpath "$(command -v nvcc)")")")
declare -A thresholds=([inline5]=5 [inline250]=250)

build() {
    local name=$1 start=$SECONDS
    cmake -S . -B "$work/$name" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$clang" \
        -DCMAKE_CUDA_COMPILER="$clang" \
        -DCMAKE_CUDA_COMPILER_LAUNCHER="$python;$repo/tools/cuda_compile.py;compile" -DCUDAToolkit_ROOT="$cuda" -DCMAKE_CUDA_ARCHITECTURES=70 \
        -DSPIRE_CUDA_INLINE_THRESHOLD="${thresholds[$name]}" -DPython_EXECUTABLE="$python" -DBUILD_TESTING=ON \
        > "$work/$name.configure.log" 2>&1
    cmake --build "$work/$name" --parallel 2 --target spire spire_gpu test_cuda_parity \
        > "$work/$name.build.log" 2>&1
    "$python" tools/cuda_compile.py report "$work/$name"
    echo "$((SECONDS - start))" > "$work/$name.seconds"
}

echo "building variants sequentially, two compiler processes per build; logs in $work"
for name in inline5 inline250; do
    build "$name"
    echo "== $name: build $(cat "$work/$name.seconds") s"
    "$work/$name/test_cuda_parity"
    PYTHONPATH="$work/$name:$repo" "$python" tools/benchmark_cuda.py --sizes 2048 8192 32768 --seconds 5 \
        | tee "$work/$name.throughput.jsonl"
done
