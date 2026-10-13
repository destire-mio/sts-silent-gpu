# Design

## Target
Slay the Spire 1, Steam build `[V2.3.4] (12-18-2022)`, `desktop-1.0.jar` SHA-256
`dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a`.
The Silent, Ascension 20, everything unlocked, Act 4 available, standard mode, a full run from
Neow to the Heart. Out of scope: other characters, daily/custom modifiers, save/load, unlock
progression, anything visual. Content that only reaches the Silent through other colours
(Prismatic Shard) is listed in the inventory and ported last.

Exactness: same seed + same actions => same snapshot as the original at every decision point.

## Engine (`core/`)
One `Game` struct holds a whole run: run state, combat state and every RNG stream. It is plain
data with fixed capacities, so a batch is an array of `Game` and a GPU thread owns one element.

Entry points, all deterministic and free of global state:
- `reset(Game&, seed)`
- `legal_actions(const Game&, ActionSet&)`
- `step(Game&, Action)`: applies one player decision and runs the game until the next decision or
  the end of the run
- `snapshot(const Game&, ...)`: host only, see below

Device C++: the code under `core/` must compile both as host C++17 and inside CUDA kernels.
- every function carries `SPIRE_FN` (`__host__ __device__` under CUDA, empty otherwise)
- no heap, no exceptions, no RTTI, no virtual functions, no recursion, no function pointers
- no standard library beyond `<cstdint>`, `<cstddef>` and fixed-size helpers defined in `core/`
- no mutable globals or statics; constant tables are `constexpr` arrays
- capacities (cards per pile, monsters, queued actions, powers) are constants sized from the
  game's real limits; overflow is `SPIRE_ASSERT`, never silent truncation

Faithfulness comes from keeping the game's own structure:
- the action queue, with `addToBottom` / `addToTop`, the pre-turn and card queues, and the
  order in which `GameActionManager` drains them
- power, relic and card hook order as the Java iterates them (player powers, relics, hand,
  monsters, in list order); lists stay ordered lists
- the damage and block calculation order of `AbstractCard` / `DamageInfo`, including float
  arithmetic and `MathUtils.floor`
- `com.megacrit.cardcrawl.random.Random` over `RandomXS128`, one stream per Java stream, same
  call count on every path. A missed or extra RNG call is a bug even if the state still matches.
- logic that uses unseeded randomness (`MathUtils.random`, `Collections.shuffle` without a seeded
  `Random`) is listed in `inventory/unseeded.md`; the oracle pins those calls so both sides agree

Numbers that are data (cost, damage, block, magic number, upgrade deltas, rarity, type, target)
live in constant tables. Behaviour is a `switch` on the id that follows the Java method.

## Snapshot and trace
A snapshot is the canonical JSON picture of one decision point. The oracle defines it; the engine
emits the same shape; tests compare them key by key with no tolerance.

It holds everything that can affect a future decision or outcome:
- run: seed, floor, act, ascension, HP, max HP, gold, keys, master deck (id, upgrades, misc, in
  order), relics (id, counter, in order), potions by slot, map, current node, boss ids
- screen: which decision is being asked and its options
- combat: energy, block, player powers in order, stance-free player state, hand / draw / discard /
  exhaust piles in order with per-card cost and modified values, each monster (id, HP, block,
  powers in order, intent, move history), turn number, cards played this turn
- rng: every stream's call counter

A trace is a JSONL file: a header (seed, character, ascension, any scenario setup), then one line
per decision with `snapshot`, `legal` (the legal actions in canonical order) and `action` (the
one taken). The last line is the terminal snapshot.

Actions are canonical and UI-free: `play(card_index, target_index)`, `end_turn`,
`potion_use(slot, target)`, `potion_discard(slot)`, `choose(option_index)`, `proceed`, `skip`.

## Tests
`tests/` replays a trace through the engine: `reset` (or apply scenario setup), then for each line
compare snapshot and legal actions, then `step` with the recorded action. The first difference is
reported with its JSON path and both values. One metric tracks the whole project: the share of
traces that replay with zero differences, split by unit in `inventory/`.

## Oracle (`oracle/`)
Drives the unmodified game logic headless and writes traces. Two kinds:
- scenario traces: a chosen deck, relics, potions and encounter, to cover each unit in isolation
- full-run traces: whole games from a seed under a seeded random legal policy

## Inventory (`inventory/`)
One row per unit in scope (card, relic, potion, power, monster, encounter, event, room/screen
rule, ascension modifier) with its Java path. Status is computed from test results, never typed
by hand.
