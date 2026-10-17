#!/usr/bin/env bash
# One-off comparison: default device inlining against -inline-threshold=5.
# Builds both side by side, checks CPU/GPU parity for each, then measures engine speed on one GPU.
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
jobs=$(( $(nproc) / 2 ))
declare -A flags=([default]="" [inline5]="-Xarch_device -mllvm=-inline-threshold=5")

build() {
    local name=$1 start=$SECONDS
    cmake -S . -B "$work/$name" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$clang" \
        -DCMAKE_CUDA_COMPILER="$clang" -DCUDAToolkit_ROOT="$cuda" -DCMAKE_CUDA_ARCHITECTURES=70 \
        -DCMAKE_CUDA_FLAGS="${flags[$name]}" -DPython_EXECUTABLE="$python" -DBUILD_TESTING=ON \
        > "$work/$name.configure.log" 2>&1
    cmake --build "$work/$name" --parallel "$jobs" --target spire spire_gpu test_cuda_parity \
        > "$work/$name.build.log" 2>&1
    echo "$((SECONDS - start))" > "$work/$name.seconds"
}

echo "building both variants at once, $jobs jobs each; logs in $work"
build default & first=$!
build inline5 & second=$!
wait "$first"; wait "$second"
for name in default inline5; do
    echo "== $name: build $(cat "$work/$name.seconds") s"
    "$work/$name/test_cuda_parity"
    PYTHONPATH="$work/$name:$repo" "$python" tools/benchmark_cuda.py --sizes 2048 8192 32768 --seconds 5 \
        | tee "$work/$name.throughput.jsonl"
done
