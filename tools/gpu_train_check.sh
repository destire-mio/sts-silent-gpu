#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
build=$(realpath -m "${1:?usage: tools/gpu_train_check.sh BUILD_DIR FRESH_OUTPUT_DIR AUDITED_E2_ROOTS_FILE}")
output=$(realpath -m "${2:?supply a fresh output directory}")
roots=$(realpath "${3:?supply the audited E2 roots.jsonl.gz}")
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo"
test ! -e "$output"
test -f "$roots"
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 PYTHONDONTWRITEBYTECODE=1
python=${SPIRE_PYTHON:?set SPIRE_PYTHON to the CUDA-enabled server venv Python}
exec "$python" -m tools.gpu_train_check suite --roots "$roots" --build "$build" --output "$output"
