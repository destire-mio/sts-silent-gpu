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

The collector finishes a fixed group of starts under one frozen policy before PPO updates.
Completed episodes use full-episode Monte-Carlo value targets with gamma=1; default GAE
lambda=1 gives advantage `outcome - old_value`. `--gae-lambda` changes advantages only;
completed value targets remain the actual 0/1 result. A decision-cap cut bootstraps the
frozen final-observation value and is labeled truncated, excluded from win/loss calibration.
Faulted trajectories are excluded from PPO and retained in episode logs. Host core assertions
still abort the process; there is no signal recovery.

PPO stores old chosen-action log probabilities and complete legal candidate sets in compact
observation records. Only selected minibatches are padded by `ObservationBatch`. The clipped
policy objective uses advantage normalization, value MSE, entropy divided by log(legal
candidate count), and gradient clipping. Single-candidate entropy is zero. `ppo.kl` is the
nonnegative sampled approximation `mean(ratio - 1 - log_ratio)` across minibatches, not an
exact distribution KL or an automatic stopping rule.

Use one CPU thread and keep generated artifacts outside the checkout:

```sh
export SPIRE_PYTHON=/Users/destire/Documents/Codex/2026-09-10/new-chat-2/outputs/spire-lab/.venv/bin/python
export PYTHONPATH=/private/tmp/trainer-runs/build:.
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 VECLIB_MAXIMUM_THREADS=1
"$SPIRE_PYTHON" -m train.run --output /private/tmp/trainer-runs/natural \
  --updates 2 --episodes 12 --batch-size 4

"$SPIRE_PYTHON" -m train.roots --output /private/tmp/trainer-runs/roots.json
"$SPIRE_PYTHON" -m train.run --roots /private/tmp/trainer-runs/roots.json \
  --output /private/tmp/trainer-runs/deep --updates 2 --episodes 6 --batch-size 3
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
`--episodes` is starts per update, `--minibatch-size` is decisions per gradient minibatch.
`--decision-cap` counts continuation decisions, excluding prefix replay. Compact storage has
a fixed `--storage-mib` budget; exhaustion aborts without a partial update. The defaults are
for small runs, not a pre-approved large experiment.

Each fresh output directory contains config/root inputs, initial and latest checkpoints,
update JSONL metrics and episode candidate-index logs for replay/audit. Checkpoints include
model, optimizer, configuration, observation version and engine Git commit. Logs report
starts/completions/wins, independent families won, natural starts, faults/truncations, new and
replay decisions, per-screen normalized entropy, sampled KL, start/decision value calibration
with screen/act breakdowns, compact bytes and peak RSS. Wall-time buckets are engine+encode
(including root replay), inference (including tensor transfer/sampling), storage, PPO update
(including padding/transfer), and other work. Worker reset/step/encode times are additional
detail, not added again to wall time.

Frozen evaluation samples one continuation per root seed using an independent per-seed PCG64
stream. It retains faults/truncations in the denominator; ordinary Wilson intervals are
reported only if every attempt terminates, otherwise it reports bounds with unresolved
outcomes. Paired reports require identical roots, sampling stream, cap and evaluation engine.

```sh
"$SPIRE_PYTHON" -m train.evaluate --checkpoint /private/tmp/trainer-runs/natural/initial.pt \
  --episodes 8 --output /private/tmp/trainer-runs/before.json
"$SPIRE_PYTHON" -m train.evaluate --checkpoint /private/tmp/trainer-runs/natural/checkpoint.pt \
  --episodes 8 --reference /private/tmp/trainer-runs/before.json \
  --output /private/tmp/trainer-runs/after.json
"$SPIRE_PYTHON" -m train.evaluate --checkpoint /private/tmp/trainer-runs/deep/checkpoint.pt \
  --roots /private/tmp/trainer-runs/roots.json --root-index 1 \
  --output /private/tmp/trainer-runs/deep-32-eval.json
```

Duplicate seed families are rejected in one evaluation; use one distance per E1 root task.
Repeated policy-stream trials on that root are not independent game seeds. The CLI defaults
to development seeds; sealed evaluation requires an explicit partition and seed selection
after policy/configuration freeze. These smoke commands do not execute the E1 three-seed
learning experiment or establish its success thresholds.

`--device cuda` moves the same network and optimizer code to CUDA. Tensor allocation and
timing synchronization use that device. The current `TrainingBatch` engine remains on CPU;
GPU-resident engine integration and CUDA execution/parity measurements are separate work.

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
- `train/`: visible-observation entity policy, synchronous PPO, roots and frozen evaluation.
- `cuda/`: conditional CUDA batch kernel.
- `tests/`: native checks, device discipline checker and strict trace replayer.
- `oracle/`: original-game trace recorder developed on its own branch.
- `inventory/`: content inventory and trace-derived status developed on its own branch.
