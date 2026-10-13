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

## Layout

- `core/`: fixed-capacity C++17 run and combat state, RNG, queues and per-unit content.
- `host/`: CPU batch driver, Python binding, snapshot serialization and benchmark.
- `cuda/`: conditional CUDA batch kernel.
- `tests/`: native checks, device discipline checker and strict trace replayer.
- `oracle/`: original-game trace recorder developed on its own branch.
- `inventory/`: content inventory and trace-derived status developed on its own branch.
