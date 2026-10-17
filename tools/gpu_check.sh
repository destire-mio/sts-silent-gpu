#!/usr/bin/env bash
set -euo pipefail
build=$(realpath -m "${1:?usage: tools/gpu_check.sh BUILD_DIR E2_ROOTS_FILE}")
roots=$(realpath "${2:?supply the audited E2 roots jsonl.gz}")
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo"
test ! -e "$build/CMakeCache.txt"
test -f "$roots"
mkdir -p "$build/tmp"
export TMPDIR="$build/tmp" CUDA_VISIBLE_DEVICES=0
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
python=${SPIRE_PYTHON:?set SPIRE_PYTHON to the server venv Python}
nvcc=$(command -v nvcc)
clang=$(command -v clang++)
"$nvcc" --version
"$clang" --version
nvidia-smi
"$python" -c 'import torch; assert torch.cuda.is_available() and torch.cuda.device_count() == 1; assert "sm_70" in torch.cuda.get_arch_list(), torch.cuda.get_arch_list(); print(torch.__version__, torch.version.cuda, torch.cuda.get_device_name())'
cmake -S . -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$clang" \
    -DCMAKE_CUDA_COMPILER="$nvcc" -DCMAKE_CUDA_HOST_COMPILER="$clang" \
    -DCMAKE_CUDA_ARCHITECTURES=70 -DPython_EXECUTABLE="$python" -DBUILD_TESTING=ON
cmake --build "$build" --parallel 1
export PYTHONPATH="$build:$repo"
ctest --test-dir "$build" --output-on-failure --no-tests=error -j1
"$python" tests/check_cuda_roots.py "$roots"
"$python" tools/benchmark_cuda.py --sizes 32 128 512 2048 8192 32768 --seconds 5 | tee "$build/throughput.jsonl"
