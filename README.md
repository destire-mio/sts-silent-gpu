# sts-silent-gpu

A C++ engine targeting exact Slay the Spire 1 behavior for the Silent (Ascension 20, three keys,
Heart), with one complete `Game` owned by each GPU thread.

The target is the original Java build `[V2.3.4] (12-18-2022)`: the same seed and actions must
produce the same snapshot at every decision. See [the design](docs/design.md) and [rules](AGENTS.md).

## Build and checks

On this macOS arm64 machine, use the existing Python environment:

```sh
SPIRE_PYTHON=/Users/destire/Documents/Codex/2026-09-10/new-chat-2/outputs/spire-lab/.venv/bin/python
cmake -S . -B /private/tmp/spire-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DPython_EXECUTABLE="$SPIRE_PYTHON"
cmake --build /private/tmp/spire-build --parallel 1
ctest --test-dir /private/tmp/spire-build --output-on-failure
```

Every build checks `core/` headers and the clang AST for device restrictions, including mutable
globals, missing `SPIRE_FN`, heap allocation, exceptions, function pointers and recursion. Native
checks cover RNG vectors, monster behavior and combat; Python checks cover the host API and trace
comparison. Host floating point contraction is disabled. `cuda/batch.cu` provides one thread per
`Game`, stepping a batch of actions, and is built only when CMake finds a CUDA compiler. CUDA has
not been compiled or run on this machine.

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
`reset` initializes run data; map and Neow decisions are outside this implementation.

`start_combat` requires an encounter key from `spire.encounters`. Omitted deck and relic fields use
the Silent A20 starter setup; an explicit empty list stays empty. Card entries use Java IDs and
support `upgrades` and `misc`. Relics use Java ID strings. Empty potion slots are `None`; nonempty
potions are unsupported and raise an error. Additional scenario fields are `floor`, `gold` and
`unseeded_seed`. Defaults are 59 HP, 66 max HP, floor 1, 99 gold and unseeded seed 0.
Seeds accept Java signed 64-bit integers.

Actions are dictionaries: `{"type": "play", "card": 0, "target": 0}`,
`{"type": "end_turn"}` or `{"type": "choose", "option": 0}`. Non-targeted cards use `target: -1`.
Use the returned legal actions in their existing order. Invalid core actions abort through
`SPIRE_ASSERT`.

For CPU batches, `batch = spire.Batch(count)` owns independent games. Call
`batch.reset(seeds)` or `batch.start_combat(seeds, scenario)`, then `batch.legal_actions()`,
`batch.step(actions)` and `batch.snapshot()`. Supply one seed or action per game. Batch stepping
uses one CPU thread and retains the Python GIL.

## Trace replay

```sh
PYTHONPATH=/private/tmp/spire-build "$SPIRE_PYTHON" tests/replay.py /path/to/traces
```

The replayer accepts JSONL files or directories. A header supplies `seed`, `character`,
`ascension` and an optional `scenario`. Each decision has `snapshot`, `legal` and `action`; the
last record has a terminal `snapshot`. Comparison includes every key, scalar type, value and
list position, without tolerance or field filtering. Failures report the first differing JSON
path and both values. Output reports passing traces overall and by header `unit`, otherwise
scenario encounter or `full_run`.

The current host snapshot is a diagnostic `run` / `screen` / `combat` / `rng` structure. It
includes card identities, ordered piles and powers, monster state, pending action queues and RNG
states/counters. The independently developed oracle currently emits a Java object graph; these
schemas do not match. Its pinned unseeded RNG contract also differs from the current core.
Oracle replay acceptance is pending resolution of both contracts and zero-difference traces.
Self-generated host traces test replayer wiring; they do not establish agreement with the game.

## CPU throughput

After configuring the build, run:

```sh
cmake --build /private/tmp/spire-build --target benchmark --parallel 1
```

This prints the elapsed time and decisions per second for 100,000 steps, one CPU thread, a seeded
random legal policy and a cycle through the implemented Act 1 weak/strong encounters. It includes
combat resets. To choose the decision count, run `/private/tmp/spire-build/spire_benchmark 1000000`.
Results are written to stdout.

## Layout

- `core/`: fixed-capacity C++17 game state, RNG, queues and combat content.
- `host/`: CPU batch driver, Python binding, snapshot serialization and benchmark.
- `cuda/`: conditional CUDA batch kernel.
- `tests/`: native checks, device discipline checker and strict trace replayer.
- `oracle/`: original-game trace recorder developed on its own branch.
- `inventory/`: content inventory and trace-derived status developed on its own branch.
