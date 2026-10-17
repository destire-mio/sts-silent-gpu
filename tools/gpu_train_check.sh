#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
build=$(realpath -m "${1:?usage: tools/gpu_train_check.sh FRESH_BUILD_DIR ROOTS_FILE}")
roots=$(realpath "${2:?supply an E1 JSON or audited E2 roots.jsonl.gz}")
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo"
test ! -e "$build"
test -f "$roots"
mkdir -p "$build/tmp"
export TMPDIR="$build/tmp" CUDA_VISIBLE_DEVICES=0,1,2,3,4,5,6,7
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 PYTHONDONTWRITEBYTECODE=1
python=${SPIRE_PYTHON:?set SPIRE_PYTHON to the CUDA-enabled server venv Python}
nvcc=$(command -v nvcc)
clang=$(command -v clang++)
"$nvcc" --version
"$clang" --version
nvidia-smi
"$python" -c 'import torch; assert torch.cuda.device_count() == 8; assert "sm_70" in torch.cuda.get_arch_list(); assert all(torch.cuda.get_device_capability(i) == (7,0) for i in range(8)); print(torch.__version__, torch.version.cuda, [torch.cuda.get_device_properties(i) for i in range(8)])'
cmake -S . -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$clang" \
    -DCMAKE_CUDA_COMPILER="$nvcc" -DCMAKE_CUDA_HOST_COMPILER="$clang" \
    -DCMAKE_CUDA_ARCHITECTURES=70 -DPython_EXECUTABLE="$python" -DBUILD_TESTING=ON
cmake --build "$build" --parallel 1
export PYTHONPATH="$build:$repo"
ctest --test-dir "$build" --output-on-failure --no-tests=error -j1
"$python" -m tools.gpu_train_check suite --roots "$roots" --output "$build" | tee "$build/training-check.jsonl"
