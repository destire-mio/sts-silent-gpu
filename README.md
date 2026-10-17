# sts-silent-gpu

A C++ engine targeting exact Slay the Spire 1 behavior for the Silent (Ascension 20, three keys,
Heart), with one complete `Game` owned by each GPU thread.

The target is the original Java build `[V2.3.4] (12-18-2022)`: the same seed and actions must
produce the same snapshot at every decision. See [the design](docs/design.md), [rules](AGENTS.md),
and [content porting guide](docs/porting.md).

## Build and checks

On this macOS arm64 machine, use the existing Python environment:

```sh
SPIRE_PYTHON=/Users/destire/Documents/Codex/2026-09-10/new-chat-2/outputs/spire-lab/.venv/bin/python
cmake -S . -B /private/tmp/spire-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DPython_EXECUTABLE="$SPIRE_PYTHON"
cmake --build /private/tmp/spire-build --parallel 1
ctest --test-dir /private/tmp/spire-build --output-on-failure
```

Every build checks `core/` and `obs/` headers and the clang AST for device restrictions, including mutable
globals, missing `SPIRE_FN`, heap allocation, exceptions, function pointers and recursion. Native
checks cover RNG vectors, monster behavior and combat; Python checks cover the host API and trace
comparison. Host floating point contraction is disabled. `cuda/batch.cu` provides one thread per
`Game`, stepping a batch of actions, and is built only when CMake finds a CUDA compiler.
Clang's device-LLVM smoke check covers the engine and observation encoder. NVCC compilation
and execution on GPU hardware have not been tested on this machine.

## V100 day-one check

Server target: Linux x86_64 (Ubuntu 22.04 or 24.04), eight V100 GPUs (`sm_70`, 16 GB each),
CUDA Toolkit **12.8.x or 12.9.x** including NVCC and development headers (CUDA 13 no longer
compiles for `sm_70`), NVIDIA driver **570.124.06 or newer** (the cluster has 580.105.08),
Clang **17 or 18** with libstdc++ development headers, CMake **3.24 or newer**,
Make, rsync, and Python **3.12** with development headers. These are the intended toolchain
bounds, not a GPU validation claim. NVIDIA documents the
[CUDA 12.8 compiler/OS requirements](https://docs.nvidia.com/cuda/archive/12.8.1/cuda-installation-guide-linux/index.html)
and [driver versions](https://docs.nvidia.com/cuda/archive/12.8.1/cuda-toolkit-release-notes/index.html).

The local venv has Python **3.12.13**, `numpy==2.5.3`, `pybind11==3.1.0`, and
`torch==2.14.0`. Recreate these package versions in a Linux venv; PyTorch must be a CUDA-enabled
build compatible with the installed driver whose kernels include V100 (check `torch.version.cuda`,
`torch.cuda.is_available()` and that `torch.cuda.get_arch_list()` contains `sm_70`). Do not copy the macOS venv. Game JARs, Java, Steam and oracle
runtime assets are not needed to build, replay the committed recordings or run GPU checks.
No dependencies are downloaded by the check script.

From the Mac, after choosing the SSH alias and the accepted E2 root file:

```sh
GPU_HOST=your-gpu-alias
E2_ROOTS=../sts-silent-gpu-agents/artifacts/e2-roots/native-audit.roots.jsonl.gz
rsync -az --exclude='.git/' --exclude='oracle/instance' --exclude='build*/' \
  --exclude='.venv/' --exclude='*.so' --exclude='*.o' \
  --exclude='__pycache__/' --exclude='/traces/' \
  ./ "$GPU_HOST:sts-silent-gpu/"
rsync -av "$E2_ROOTS" "$GPU_HOST:spire-e2.roots.jsonl.gz"
```

Keep `tests/traces/` in the transfer (do not exclude it); the root-level ignored `traces/`
contains generated recordings. On the server, in the synced checkout:

```sh
export SPIRE_PYTHON="$HOME/spire-venv/bin/python"
tools/gpu_check.sh "$HOME/spire-gpu-build" "$HOME/spire-e2.roots.jsonl.gz"

# Complete training path: all eight devices, a separate fresh directory.
tools/gpu_train_check.sh "$HOME/spire-train-build" "$HOME/spire-e2.roots.jsonl.gz"
```

`gpu_train_check.sh` requires eight visible V100s and builds for `sm_70` with one compiler
process; it downloads nothing. E1 `train.json` can replace the E2 root file for this check.
It runs full CTest, then compares the same fp32 policy on CPU and GPU batches: counter words,
actions, episode outcomes, compact observation bytes, masks and policy versions must agree
exactly; log probabilities, values, returns and advantages use `atol=rtol=3e-5`.
It also tests a non-default CUDA stream, invalid-prefix isolation, refill and permuted lanes.
The supplied prefixes and 32 continuation decisions per root exercise the rooted workload;
this is separate from original-game trace acceptance.

Single-GPU probes use the default 128-wide, four-layer policy, four continuation decisions
from natural roots, one PPO epoch and global minibatches of 256. Sizes are
32/128/512/2048/8192/32768/65536/131072/262144, ending at the first memory exhaustion. Each size
runs in a fresh process after warmup. An OOM is recorded as the memory boundary; any other
failure aborts. The last successful size is a measured bound for this workload, not a capacity
promise for longer episodes. JSON records contain collect/update seconds, end-to-end decisions/s,
CUDA-event engine/encode/inference/storage times, PyTorch peak allocation and device free memory
(the PyTorch peak excludes native simulator allocation).

The 1/2/8-GPU checks use the same 512 rooted attempts and initialization, cap 16, and two
collect/update versions. They compare version-0 actions/outcomes across GPU counts, assert zero
faults and version progression, and report strong scaling against the 1-GPU run, including root
replay, diagnostics and checkpoint time. Policy inference, gradients and Adam use fp32; V100 bf16 is not used. CDFs, target scans and
normalization accumulators use float64 to preserve their scalar-reference arithmetic.
Logs and JSON are under the supplied build directory (`training-check.jsonl`,
`training-throughput.json`, `probe-*.log`, `ddp-*/result.json`). No GPU result is asserted by the
Mac's CPU and device-LLVM tests; NVCC, CUDA execution, memory bounds and NCCL scaling require
this command on the target server.

The original `gpu_check.sh` build directory must be fresh. That script selects GPU 0, requires CUDA, builds with one
compiler process, runs the full CTest suite including `test_cuda_faults`, `test_cuda_parity`
and the Python CUDA test, replays the supplied E2 prefixes on both batch implementations,
then measures random and deep stepping plus encoding at batch sizes 32/128/512/2048/8192/32768.
`test_cuda_parity` compares every `Game` byte and every observation/status byte after each
step of the 12 deep routes and seeded random trajectories, including reset, cap, clone,
invalid actions and seed exhaustion. Full Game downloads occur in parity tests only.
The throughput output records selection, reset, step, encode and synchronization time;
it excludes policy inference and learning. Results are saved as `BUILD_DIR/throughput.jsonl`.

The conditional `spire_gpu.TrainingBatch(size, decision_cap, seeds)` exposes the same seven
observation arrays, counts, rewards, done/truncated/fault/reset/advanced flags, seed indices
and decision counts in device memory. The returned objects implement CUDA Array Interface;
`torch.as_tensor(batch.tags, device="cuda")` creates a view that keeps its owner alive.
Use these tensors for reading; writes modify the batch buffers.
`batch.step(indices)` takes a contiguous CUDA `torch.int32` vector on the batch device,
launches on PyTorch's current stream and waits for completion. Observations are reused on
the next operation. Consume them on that stream before stepping; clone tensors to retain
an observation. The trainer computes int64 offsets with tensors and packs observations into device uint8 records;
it does not call the host `store` method or read CPU worker timing.
`step_slots`, `reset_to`, `reset_to_many`, `clone` and `extend_seeds` take the host arguments
used by `spire.TrainingBatch`; root seeds/actions are uploaded and replayed on the GPU.
Use these administrative calls between completed policy operations on the default stream.
The trainer uses `step_tensors(int32 choices[size], uint8 active[size])` and
`reset_tensors(int64 slots, int64 root_indices, int64 seeds, int64 offsets, int32 prefixes)`.
`store_tensors(int64 slots, uint8 data, int64 offsets)` writes the existing compact byte format
with one device thread per record. All inputs reside on the batch device. These calls enqueue
work on PyTorch's current stream without a device synchronization; consume observations and
retain input tensors on that stream.
Roots are uploaded once per collection and replayed on the device during refill. The CPU batch
exposes these same methods for reference tests, with NumPy observation buffers viewed as tensors.
Only read from these views; the native batch owns and mutates them.

The Mac checks restricted C++ and CUDA device LLVM generation, plus CPU execution of the
same kernel bodies. NVCC compilation, GPU byte parity, device stack/resource use and V100
throughput remain day-one checks.

## Python API

Add the build directory to `PYTHONPATH` and import `spire`:

```sh
PYTHONPATH=/private/tmp/spire-build "$SPIRE_PYTHON" -c 'import spire; print(spire.encounters)'
```

```python
import spire

scenario = {
    "encounter": "Cultist",
    "deck": [
        {"id": "Strike_G", "upgrades": 0},
        {"id": "Defend_G", "upgrades": 0},
        {"id": "Neutralize", "upgrades": 0},
        {"id": "Survivor", "upgrades": 0},
    ],
    "relics": ["Ring of the Snake"],
    "potions": [None, None],
    "hp": 59,
    "max_hp": 66,
}
game = spire.start_combat(42, scenario)
while game.legal_actions():
    action = game.legal_actions()[0]
    game.step(action)
state = game.snapshot()
```

`Game` exposes `reset(seed)`, `start_combat(seed, scenario)`, `legal_actions()`, `step(action)` and
`snapshot()`. Module functions expose the same operations: `reset(seed)` and
`start_combat(seed, scenario)` return a `Game`; the other functions take a `Game` first.
`reset` opens Neow; the same action interface drives maps, rooms, rewards and act transitions.

`start_combat` requires an encounter key from `spire.encounters`. Omitted deck and relic fields use
the Silent A20 starter setup; an explicit empty list stays empty. Card entries use Java IDs and
support `upgrades` and `misc`. Relics and potions use catalog Java ID strings. Empty potion slots
are `None`; the current potion catalog contains the empty slot. Unknown IDs raise an error.
Additional scenario fields are `floor` and `gold`. Defaults are 59 HP, 66 max HP,
floor 1 and 99 gold. The pinned unseeded stream starts from the run seed.
Seeds accept Java signed 64-bit integers. Run-level scenarios supply `start` for a later act,
floor, map node, room, gold and keys. Optional `profile` supplies Neow spirit and boss-seen
preferences. The setup contract and exact fields are in [the design](docs/design.md).

Actions are dictionaries: `{"type": "play", "card": 0, "target": 0}`,
`{"type": "end_turn"}`, `{"type": "choose", "option": 0}` or `{"type": "proceed"}`.
Hand selection requires confirmation after choosing cards. Non-targeted cards use `target: -1`.
Use the returned legal actions in their existing order. Invalid core actions abort through
`SPIRE_ASSERT`.

For CPU batches, `batch = spire.Batch(count)` owns independent games. Call
`batch.reset(seeds)` or `batch.start_combat(seeds, scenario)`, then `batch.legal_actions()`,
`batch.step(actions)` and `batch.snapshot()`. Supply one seed or action per game. Batch stepping
uses one CPU thread and retains the Python GIL.

`spire.TrainingBatch(size, threads, decision_cap, seeds)` provides preallocated NumPy entity,
candidate and mask arrays, candidate-index actions, terminal reward, truncation, fault and reset
buffers. CPU stepping releases the GIL. Its [visible-information protocol](docs/observation.md)
defines the array layout, training-only clone, seed sequence and reset semantics. Give the
policy only the seven observation arrays; `snapshot()` and `search_state()` expose hidden state.
Root-family partitions are in [the seed inventory](inventory/seed-partitions.json); evaluation
arithmetic is in `tools/evaluation.py`. The `train/` package supplies the training algorithm.

```python
import numpy as np

batch = spire.TrainingBatch(96, 4, 3000, list(range(1000000000, 1000010000)))
tags, ids, values = batch.tags, batch.ids, batch.values
kinds, refs = batch.candidate_kinds, batch.candidate_refs
entity_mask, candidate_mask = batch.entity_mask, batch.candidate_mask
actions = np.zeros(96, dtype=np.int32)
batch.step(actions)
```

After a terminal/cap/fault output, the next call resets that slot and ignores its action index.
Read final observations before that call; clear recurrent memory where `reset_mask` is set.
Run `tools/check_observation.py tests/traces --deep inventory/deep-neow.json --output /private/tmp/observations.json`
with the same `PYTHONPATH` to encode oracle decisions while replaying both comparison scopes.
For the complete 974-trace corpus, append the 12 `extra_deep_traces.path` entries from
`inventory/observation.json` to the positional paths. The audit reports decision-weighted
entity/candidate distributions and compact bytes including counts and offsets.
The padded observation is 10,184 bytes including counts. `batch.store(slots, data, offsets)`
writes valid rows to caller-owned uint8 storage with uint64 offsets; records occupy
`12 + 24*entities + 7*candidates` bytes including their offset. Use
`spire.ObservationBatch(minibatch_size).pad(data, offsets, indices)` to gather a dense minibatch.
The [protocol](docs/observation.md#compact-rollout-storage) specifies append and fault behavior.

Run `tools/benchmark_training.py --size 1024 --batches 10000 --seconds 5 --mode both` to measure 1/2/4/8 threads
for uniform candidates and the 12 full-run routes in `tests/deep_benchmark.actions`. Each
configuration runs for at least five seconds, including encoding, reset and Python dispatch.
Deep-route candidate indices are resolved before timing; the timed path steps the batch through
Neow to death/victory and restarts from the caller's repeated root sequence. The output includes
act counts, observation bytes and worker encoding/stepping/reset shares. It excludes network
inference, rollout storage and learning; timing output goes to stdout.

## E2 curriculum root collection

`tools/collect_roots.cpp` searches native `Game` copies. Roots store a signed seed and
canonical legal-action candidate indices, with separate curriculum metadata; no state bytes
or hidden-future search labels enter training. `TrainingBatch.reset_to(slot, seed, candidates)`
rebuilds the game and visible history. `batch-check` compares supplied prefixes with ordinary
batch stepping, including all observation/candidate buffers.

```sh
cmake --build /private/tmp/e2-build --target spire spire_collect_roots --parallel 1
python3 tools/e2_roots.py collect --output /private/tmp/e2-roots/round2 --name breadth \
  --offset 2300 --train 1024 --dev 512 --family-decisions 2000000 --trials 12
python3 tools/e2_roots.py audit --output /private/tmp/e2-roots/round2 --name audited \
  --inputs /private/tmp/e2-roots/round2/breadth.roots.jsonl.gz
python3 tools/e2_roots.py evaluate --output /private/tmp/e2-roots/round2 --name measured \
  --inputs /private/tmp/e2-roots/round2/audited.roots.jsonl.gz
PYTHONPATH=/private/tmp/e2-build "$SPIRE_PYTHON" tools/e2_roots.py batch-check \
  --output /private/tmp/e2-roots/round2 --name batch-check \
  --inputs /private/tmp/e2-roots/round2/measured.roots.jsonl.gz
PYTHONPATH=/private/tmp/e2-build "$SPIRE_PYTHON" tools/e2_roots.py validate \
  --output /private/tmp/e2-roots/round2 --name oracle --minimum-roots 30 \
  --inputs /private/tmp/e2-roots/round2/measured.roots.jsonl.gz
python3 tools/e2_roots.py summarize --output /private/tmp/e2-roots/round2 --name candidates \
  --inputs /private/tmp/e2-roots/round2/measured.roots.jsonl.gz \
  --audits /private/tmp/e2-roots/round2/oracle/results.json
```

Use a fresh output directory. `budget.json` starts a shared three-hour wall-clock budget;
there is no total engine-decision cap. Family decision and floor-expansion limits allocate
search effort, and every attempted decision is counted. Run at most two CPU-heavy processes
and one original-game JVM. Generated roots, traces, cost ledgers and fault prefixes stay outside
the repository; output must remain below 5 GB. Only train/dev root families are accepted.

Search keeps eight floor candidates, preferring distinct builds, and up to six distinct-score
combat-turn candidates. It saves the first observed boundary on any branch, including weak
states and branches that later die. Burning-elite reachability directs emerald routing;
sapphire trades and Act 3 campfire recall are explicit choices. Search bias and exact budgets
belong in the manifest. `extend` resumes the furthest saved boundary for families without a
Heart turn 5, prioritizing Heart positions, Act 4, then Act 3. An Act 3 boss root is eligible only with all
three keys; otherwise extension starts before the boss. Existing boundaries remain retained.
Each root receives 16 uniform continuations (4096 decisions each) and
four search continuations (16384 decisions each). Success, death and live budget cuts remain
separate metadata. No filtering uses these difficulty outcomes.

`audit` replays stored indices and checks stage/resource metadata after engine changes. It
quarantines obsolete prefixes and curse-transform previews: the original's curse-preview
branch advances `cardRng` with elapsed frames, failing the 13-frame check. Discovery excludes
that choice. Uniform difficulty sampling still draws from every legal action, but records
`frame_dependent` censoring if that preview is selected. This limitation is retained in the
manifest; the original RNG split and comparison are unchanged.

Sampling is uniform over families within each partition/stage. Groups with fewer than 20
families remain diagnostic-only, so one family never exceeds 5% in a released group. Original
validation, both replay scopes and the 13-idle-frame check gate release. The
[collection manifest](inventory/e2-roots.json) records coverage, distributions, checksums,
validation and remaining gaps.

`PYTHONPATH=/private/tmp/e2-r3-build "$SPIRE_PYTHON" tests/check_retained_death.py --output /private/tmp/retained-death-check`
records the focused seed-1000042012 input in `tests/retained_death_cache.json`, replays both
scopes, and compares another original recording with 13 extra idle frames. It serializes JVMs
with the oracle lock and requires a fresh output directory. The original's death-terminal
Leg Sweep block changes from 14 to 10 in this pair; the check fails under the frozen snapshot
contract. The manifest retains that failure as a release blocker.

## E1 training and evaluation

`train/` uses one shared PyTorch policy for all screens. Each tag and ID row has an embedding;
signed-log numeric rows have a linear projection. Masked Transformer layers mix entities.
Candidate scores use their three referenced entities, action kind and pooled context. A
sigmoid value head predicts the win probability. Defaults are width 128, four layers, four
heads and feedforward width 256, about one million parameters. `--width`, `--layers`,
`--heads` and `--feedforward` configure the size.

This is the E1 **no memory** setting: no recurrent state or learned prefix memory. Root replay
rebuilds the encoder's visible-history sidecar, then the network first observes the root.
Both heads receive only the seven observation arrays. `train/` has no search, game save/load,
reward shaping or hidden-state input.

One process owns each GPU's simulator, policy, optimizer and compact rollout. The same process
collects version v and runs its local DDP shard; PPO reads observations and sample tensors from
that GPU's storage. A barrier follows the update, then all ranks publish v+1. There are no separate
actors, policy-copy broadcasts or overlapping policy versions. Every sample and episode carries
its version; all ranks check versions before the update. This single-node design avoids copying
large observations between processes and allocates the same GPUs to inference and learning in
successive phases. DDP exchanges gradients, global sample IDs/lengths and advantage statistics.
The coordinator receives action audit logs and copies of scalar sample fields for reporting once
per collection; these host copies never feed PPO. Observation bytes stay on the collecting GPU.
Rank zero writes checkpoints after updates.

CUDA is the training default and requires `spire_gpu`. CPU is a reference/test device for this
same collector and tensor storage path, not a production simulator backend or an automatic
fallback. CPU tests use gloo; CUDA uses NCCL. CPU test runs allow at most three workers.
Episode identity, decision counts, fault/terminal/truncation handling, refill, budget selection
and segmented return/GAE scans use device tensors. No per-decision observations, logits, actions
or episode status arrays are downloaded. Python schedules batched operations; dynamic counts,
maximum lengths and storage-capacity scalars still synchronize at batch/chunk boundaries.
`--inference-chunk` (default 1024, also `PipelineConfig.inference_chunk`) bounds policy and
truncation-bootstrap batches. Each wave selects lanes in admission order for the decision budget,
then sorts the selected lanes by entity count before chunking to reduce attention padding.
Outputs scatter back to their original record positions before one simulator step for the whole
wave. This is one explicit setting, with no production autotuning. There are no Python slot loops
during stepping. CPU tests compare chunks 2 and 5 on mixed roots, refill, truncation and budget cuts:
actions, outcomes, observation bytes, record order and versions agree exactly; network floats and
derived targets use the existing floating-point tolerance.

Sampling uses Philox4x32-10 computed with integer tensor operations on the device. SHA256 of the
canonical JSON `[signed_seed,candidate_prefix]` supplies two key words (the first two eight-digit
hex groups); XOR these with the low/high words of the sampling seed. Counter words are decision
low/high then attempt low/high; all words are uint32. Ten rounds use multipliers `D2511F53` and
`CD9E8D57`, and key increments `9E3779B9` and `BB67AE85`. The first output word gives the double
uniform `(word+0.5)/2^32`; inverse CDF uses the policy's fp32 probabilities accumulated and
renormalized in float64. A root's metadata and its lane/rank do not enter the key. The same
sampling seed, root, attempt and decision therefore generate the same random choice for the
same probabilities across batch sizes, slot orders and devices. CPU/GPU floating-point policy
outputs are checked with tolerance and sampled paths with exact equality on the server;
integer RNG equality alone cannot guarantee equal paths if floating-point CDFs straddle a draw.
This scheme intentionally replaces NumPy PCG64; old sampled trajectories are not reproduced.

Completed episodes use full-episode Monte-Carlo value targets with gamma=1; default GAE
lambda=1 gives advantage `outcome - old_value`. `--gae-lambda` changes advantages only;
completed value targets remain the actual 0/1 result. A decision-cap cut bootstraps the
frozen final-observation value and is labeled truncated, excluded from win/loss calibration.
Faulted trajectories are excluded from PPO and retained in episode logs. Host core assertions
still abort the process; there is no signal recovery.

PPO stores old chosen-action log probabilities and complete legal candidate sets in compact
observation records on the device. Inference crops both entity and candidate dimensions on
that device. The learner decodes compact records into the selected batch's actual maximum
lengths, with attention and candidate masks. It never expands records to schema capacities.
Each epoch shuffles ties, orders records by 16-entity length bands and candidate counts, then
shuffles the resulting minibatches. Bucketing changes minibatch membership, not the objective
for a fixed batch. The clipped
policy objective uses advantage normalization, value MSE, entropy divided by log(legal
candidate count), and gradient clipping. Single-candidate entropy is zero. `ppo.kl` is the
nonnegative sampled approximation `mean(ratio - 1 - log_ratio)` across minibatches, not an
exact distribution KL or an automatic stopping rule.

`--minibatch-size` is the global sample count, independent of learner count. All ranks use the
same global sample order and rollout-wide advantage normalization. Sample IDs are
`attempt * decision_cap + decision_index`; training attempts must be globally unique ordinals.
A sample remains on the rank that collected it. Each rank selects its members of the shared
length-bucketed minibatch. Its mean loss is weighted by
`local_count * world_size / global_count` before DDP averages gradients; clipping and Adam
follow the reduction. An empty tail shard participates with a zero-weight graph, without
adding a training sample. This preserves unequal tails and batches smaller than world size.

Use `--torch-threads` to set PyTorch's thread count per process (default one). Keep generated artifacts outside the checkout:

```sh
export SPIRE_PYTHON=/Users/destire/Documents/Codex/2026-09-10/new-chat-2/outputs/spire-lab/.venv/bin/python
export PYTHONPATH=/private/tmp/trainer-runs/build:.
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 VECLIB_MAXIMUM_THREADS=1
"$SPIRE_PYTHON" -m train.run --output /private/tmp/trainer-runs/natural \
  --updates 2 --episodes 12 --batch-size 4 --device cpu

"$SPIRE_PYTHON" -m train.roots --output /private/tmp/trainer-runs/roots.json
"$SPIRE_PYTHON" -m train.run --roots /private/tmp/trainer-runs/roots.json \
  --output /private/tmp/trainer-runs/deep --updates 2 --episodes 6 --batch-size 3 --device cpu

# Local reference smoke (two collecting/DDP test processes).
"$SPIRE_PYTHON" -m train.run --roots /private/tmp/e1-runs/roots/train.json \
  --output /private/tmp/trainer-runs/ddp --updates 1 --episodes 48 \
  --workers 2 --batch-size 32 --device cpu

# Same launcher on one machine with eight CUDA devices.
"$SPIRE_PYTHON" -m train.run --roots /private/tmp/e1-runs/roots/train.json \
  --output /private/tmp/trainer-runs/cuda --updates 1 --episodes 48 \
  --workers 8 --batch-size 32 --device cuda
```

The build commands above can use `/private/tmp/trainer-runs/build` in place of
`/private/tmp/spire-build`. PyTorch is required in the selected Python environment.
`train.roots` checks the inventory's source-action hash, replays legal actions to candidate
indices and verifies the victory observation. Its default roots are 8, 32 and 128 decisions
before the known seed 202610180018 victory. It generates root inputs, not demonstrations for
the loss. All three belong to one historical family, outside the train/dev/locked partitions.
They can establish continuation wiring, not unseen-seed performance.

Root files have this format (every prefix starts at fresh Neow):

```json
{"observation_version": 2, "roots": [{"seed": 1000000000, "candidates": [0, 1]}]}
```

Training cycles supplied roots in order. Natural starts use consecutive train seeds; new
updates use new seeds. Except for the historical E1 winning route, training rejects roots
outside the train partition. `--init-seed` controls initialization, policy sampling and
minibatch shuffling; planned independent initializations are 1729, 2718 and 31415.
`--episodes` is total starts per update, `--batch-size` is lanes per GPU,
and `--minibatch-size` is decisions per global gradient minibatch.
`--decision-cap` counts continuation decisions, excluding prefix replay. Compact storage has
a fixed `--storage-mib` budget per GPU; exhaustion aborts without a partial update. Scalar record capacity is bounded by the storage
budget divided by the smallest legal record (4-byte header, one 24-byte entity, one 7-byte candidate),
avoiding a worst-case `episodes * decision_cap` metadata allocation. The defaults are
for small runs, not a pre-approved large experiment.

Each fresh output directory contains config/root inputs, initial and latest checkpoints,
update JSONL metrics and episode candidate-index logs for replay/audit. Checkpoints include
model, optimizer, configuration, observation version and engine Git commit. Logs report
starts/completions/wins, independent families won, natural starts, faults/truncations, new and
replay decisions, per-screen normalized entropy, sampled KL, start/decision value calibration
with screen/act breakdowns, compact bytes and coordinator peak RSS. Wall-time buckets are
collection (including transport), learner (including transport, update and checkpoint), and
coordinator work. Process startup is reported separately. Engine+encode, inference and
storage are sums of per-device work measured with CUDA events; they are not additional wall-time
buckets. PPO time is part of learner time. The reported coordinator RSS excludes workers.

`tests/test_train_policy.py` compares the tensor collector against a scalar reference, including
exact packed bytes, actions, outcomes, faults, budget cuts, refill, truncation and GAE.
`tests/test_train_packing.py` compares capacity-padded and packed logits, loss and every gradient.
`tests/test_train_distributed.py` compares 2/3 gloo ranks to one process with unequal/empty shards,
SGD and Adam. Adam can amplify roundoff in mathematically zero gradients of the final score bias
and attention key biases; those null directions are reported separately while identifiable
parameters, probabilities, values, losses, gradients and optimizer moments remain checked.
`tests/test_train_pipeline.py` covers multiple policy versions, zero-sample updates, an empty
rank, checkpoint publication and retained audit trajectories. CPU tests establish those
contracts; they do not measure GPU scaling.

Frozen evaluation uses the same Collector and counter sampler, with an independent sampling seed. It retains faults/truncations in the denominator; ordinary Wilson intervals are
reported only if every attempt terminates, otherwise it reports bounds with unresolved
outcomes. Paired reports require identical roots, sampling stream, cap and evaluation engine.

```sh
"$SPIRE_PYTHON" -m train.evaluate --checkpoint /private/tmp/trainer-runs/natural/initial.pt \
  --episodes 8 --output /private/tmp/trainer-runs/before.json --device cpu
"$SPIRE_PYTHON" -m train.evaluate --checkpoint /private/tmp/trainer-runs/natural/checkpoint.pt \
  --episodes 8 --reference /private/tmp/trainer-runs/before.json \
  --output /private/tmp/trainer-runs/after.json --device cpu
"$SPIRE_PYTHON" -m train.evaluate --checkpoint /private/tmp/trainer-runs/deep/checkpoint.pt \
  --roots /private/tmp/trainer-runs/roots.json --root-index 1 \
  --output /private/tmp/trainer-runs/deep-32-eval.json --device cpu
```

Duplicate seed families require explicit independent attempt IDs in the Python evaluator.
Repeated policy-stream trials on that root are not independent game seeds. The CLI defaults
to development seeds; sealed evaluation requires an explicit partition and seed selection
after policy/configuration freeze. These smoke commands do not execute the E1 three-seed
learning experiment or establish its success thresholds.

`spire_collect_roots winning` accepts the same root-prefix input as `evaluate`; its family
budget is per search attempt and its trial argument is the number of independent search
streams. It emits successful legal prefixes and every attempt's outcome and decision cost.
The first four streams reproduce E2's four-sample combat search; later streams use sixteen
samples per beam member. These prefixes discover curriculum roots, never PPO action labels.
`tools/e1_roots.py --inputs ... --output ...` cuts exact 8/32/128-decision prefixes and measures
untrained and uniform policies. Its selected train/dev JSON files feed `python -m train.e1`:

```sh
"$SPIRE_PYTHON" -m train.e1 --roots /private/tmp/e1-runs/roots/train.json \
  --dev-roots /private/tmp/e1-runs/roots/dev.json --output /private/tmp/e1-runs/init-1729 \
  --init-seed 1729 --decisions 200000 --deadline-epoch "$E1_DEADLINE_EPOCH"
```

Set `E1_DEADLINE_EPOCH` to the experiment's absolute Unix deadline. E1 samples families uniformly, then their
available distance bands uniformly. It records 200 independent policy attempts per fixed
train/dev evaluation set, with separate distance bands before and after training and overall
curves at 50k-decision intervals. At the exact decision budget it excludes unfinished
trajectories from PPO, preserving their separate budget-cutoff outcomes. It reserves fifteen
minutes before the wall deadline and stops on any fault or per-episode truncation.
Repeated attempts measure these fixed roots, not independent game-seed coverage.

The [E1 pilot inventory](inventory/e1.json) records the frozen protocol, root hashes,
three initializations, learning curves, throughput, original-game checks and artifact paths.

## Watch a checkpoint in the original game

`watch/` connects a thin ModTheSpire adapter to one Python `spire.Game`. Every decision and
terminal compares both full oracle snapshot scopes and canonical legal actions before another
action is sent. Only the existing seven visible-observation arrays reach `train.model.Policy`;
the game seed and RNG remain in the synchronizer. Sampling uses an independent PCG64 stream.

Use the build commands above with `/private/tmp/mod-work/build` as the build directory, then
run from this checkout:

```sh
export PYTHONPATH=/private/tmp/mod-work/build:.
python3 -m watch.build --output /private/tmp/mod-work/mod
"$SPIRE_PYTHON" -m watch.check --mod /private/tmp/mod-work/mod/Watch.jar --output /private/tmp/mod-work/check
"$SPIRE_PYTHON" -m watch.run --mod /private/tmp/mod-work/mod/Watch.jar \
  --checkpoint /private/tmp/e1-runs/init-2718/checkpoint.pt \
  --roots /private/tmp/e1-runs/roots/train.json --root-index 7 \
  --delay 0.5 --output /private/tmp/mod-work/watch
```

Use `--seed 1` in place of `--roots ... --root-index ...` to begin at Neow. E1 JSON root lists
and E2 `.roots.jsonl.gz` records are accepted; indices start at zero. Prefixes replay from
Neow with graphics at accelerated speed. Policy play uses 60 FPS and `--delay` seconds before
each action. Waiting freezes game updates, preserving the compared state. This avoids spending
the viewing session watching the training root's long prefix. Graphics are silent and inputs
are controlled by the adapter; closing the window stops the run.

Each output directory must be fresh. It owns the game's working directory, home, preferences,
saves and temporary files; installed JARs/runtime are read-only links. The build reuses
`Actions`, `Choices`, `Snapshot` and the recorder's start/settlement, and redirects only adapter
classes, preserving the 18 audited game-class patches and RNG split. `watch.check` runs seed 1's
30 scripted decisions headless and replays the resulting trace through both scopes.
`result.json`, `trace.jsonl.gz`, `input.json` and `game.log` retain the outcome and inputs.
A divergence stops action execution, prints the JSON path, retains both states in
`difference.json`, and holds the game window with the difference until closed. Successful
terminal comparison displays the outcome for five seconds before exit. An unfinished trace
from a divergence is not a passing replay.
The [watch validation inventory](inventory/watch.json) records the seed-1 check and the E1
root-7 Heart victory, including checkpoint/trace hashes and fresh-build replay results.

## Trace replay

```sh
PYTHONPATH=/private/tmp/spire-build "$SPIRE_PYTHON" tests/replay.py /path/to/traces
```

The replayer accepts JSONL, gzip-compressed JSONL and directories. A header supplies `schema`,
`game_sha256`, `seed`, `character`, `ascension` and `scenario` (null for a full run).
Each decision has `snapshot`, `legal` and `action`; the last record has `snapshot` and `terminal`.
Comparison includes every key, scalar type, value and
list position, without tolerance or field filtering. Failures report the first differing JSON
path and both values. Output reports passing traces and matched decisions, including matching
decisions after earlier differences. Directory discovery includes scenario and full-run traces.
Both scopes and the terminal record must match for every fixture, including `full-run-1`. See [the porting guide](docs/porting.md) for recording and replay commands.

The host snapshot uses the oracle's object graph and preserves object references, UUID equality,
ordered fields and piles, queues and RNG counters/state words. `game.schema_gaps()` lists fields
whose engine state or serializer is missing; these fields remain absent and fail strict replay.
The committed scenario fixtures are the regression gate. Units without a triggering original-game
trace remain unaccepted. Self-generated host traces test replayer wiring; they do not establish
agreement with the game.

## CPU throughput

After configuring the build, run:

```sh
cmake --build /private/tmp/spire-build --target benchmark --parallel 1
```

This prints `sizeof(Game)` in bytes, elapsed time and decisions per second for 100,000 steps,
one CPU thread, a seeded random legal policy and a cycle through the encounter catalog.
The combat deck and starting HP are fixed so adding run initialization does not change the
benchmark load. Timing includes complete run resets and dungeon generation. To choose the decision count, run `/private/tmp/spire-build/spire_benchmark 1000000`.
Results are written to stdout.

`spire_benchmark` measures starter combats with a complete dungeon reset per combat.
For whole-game throughput, run `/private/tmp/spire-build/spire_deep_benchmark 1000`.
This replays 12 fixed Neow-to-terminal games (15,038 decisions per round) on one thread.
Timing includes resets and native `step`, excluding file loading and the initial legal-action
and terminal check. The optional second argument selects an action file. The output checksum
consumes terminal floor, HP and screen; strict snapshot parity remains the trace replayer's job.

`tests/deep_benchmark.actions` stores a seed and decision count followed by integer triples
`ActionKind, index, target` in recorded order. Unused indices/targets are -1. The nine
202610230xxx games with more than 800 decisions come from `perf-evidence/deep-actions/`:
000, 004, 005, 006, 007, 008, 010, 013, 014. Seeds 202610210030 (Dead Branch),
202610211131 (card residency) and 202610180018 (victory) come from the accepted trace actions
in `inventory/deep-neow.json`. These are input actions, not replacement oracle snapshots.
`inventory/benchmark.json` records the source paths, hashes and per-game decision counts.

## Layout

- `core/`: fixed-capacity C++17 run and combat state, RNG, queues and per-unit content.
- `obs/`: device-C++ visible-information encoder and fixed numeric schema.
- `host/`: CPU batch driver, Python binding, snapshot serialization and benchmark.
- `train/`: visible-observation entity policy, device collectors, DDP PPO, roots and frozen evaluation.
- `cuda/`: conditional CUDA batch kernel.
- `tests/`: native checks, device discipline checker and strict trace replayer.
- `oracle/`: original-game trace recorder developed on its own branch.
- `watch/`: original-game checkpoint viewer, reusing the oracle adapter and comparator.
- `inventory/`: content inventory and trace-derived status developed on its own branch.
