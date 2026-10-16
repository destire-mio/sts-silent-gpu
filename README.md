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
arithmetic is in `tools/evaluation.py`. There is no training algorithm in this interface.

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

`tools/collect_roots.cpp` searches native `Game` copies; `tools/e2_roots.py` stores gzip JSONL
outside the checkout. A root contains only a signed seed and canonical candidate indices.
Deck/relic signatures, resources and continuation results are metadata, never restored state
or training labels. Replay each prefix through `TrainingBatch(1, 1, 8192, [seed]).step(indices)`
to rebuild the visible history. No reset-to-root binding is required.

```sh
cmake --build /private/tmp/e2-build --target spire spire_collect_roots --parallel 1
python3 tools/e2_roots.py collect --name collection --output /private/tmp/e2-roots --train 512 --dev 256 --decisions 6000000
python3 tools/e2_roots.py evaluate --name difficulty --inputs /private/tmp/e2-roots/collection.roots.jsonl.gz --decisions 2000000
PYTHONPATH=/private/tmp/e2-build "$SPIRE_PYTHON" tools/e2_roots.py batch-check --name replay --inputs /private/tmp/e2-roots/difficulty.roots.jsonl.gz
python3 oracle/build.py
PYTHONPATH=/private/tmp/e2-build "$SPIRE_PYTHON" tools/e2_roots.py validate --name original --inputs /private/tmp/e2-roots/difficulty.roots.jsonl.gz
python3 tools/e2_roots.py summarize --name candidates --inputs /private/tmp/e2-roots/difficulty.roots.jsonl.gz --audits /private/tmp/e2-roots/original/results.json
```

Configure `/private/tmp/e2-build` using the build instructions above. Use a fresh output directory
and unique command names. Run one command at a time; original recordings share
`/private/tmp/spire-oracle.lock`. The ledger includes all search branches, failed searches,
difficulty continuations, prefix replays and validation replays. Native runs stop at their
decision/CPU allocation and preserve the pending prefix in a mapped journal on an assertion.
`extend --inputs ...` continues from the latest saved boundary in each existing family; its
prefix replay consumes budget too. `--offset` selects unused train/dev seeds from the inventory;
locked seeds never enter the engine.

Selection keeps the first boundary reached per family/stage before difficulty is measured.
Stage/partition groups below 20 families remain diagnostic-only, enforcing the 5% family cap
without duplicates. Search measurements use four independent policy RNG streams with 512
decisions each; uniform measurements use sixteen streams with a 2048-decision cap. All share
the root's fixed game RNG. Search success is a budgeted discovery rate, not a policy label or
proof that a zero-success root is unwinnable. The first collection added Act 1 boss starts
after Act 3/Heart proved sparse; [its manifest](inventory/e2-roots.json) records the exact
budgets, selection changes, coverage, checksums and unresolved original-game differences.
Any original validation failure blocks release of the collection for curriculum use.

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
- `cuda/`: conditional CUDA batch kernel.
- `tests/`: native checks, device discipline checker and strict trace replayer.
- `oracle/`: original-game trace recorder developed on its own branch.
- `inventory/`: content inventory and trace-derived status developed on its own branch.
