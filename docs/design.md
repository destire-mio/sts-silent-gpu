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
The integration size target is 35 KiB per `Game`, with a 48 KiB compile-time budget for later
content and rules. Per-unit payload budgets
and the build-time member breakdown are described in `porting.md`.

Entry points, all deterministic and free of global state:
- `reset(Game&, seed, RunProfile = {})`
- `legal_actions(const Game&, ActionSet&)`
- `step(Game&, Action)`: applies one player decision and runs the game until the next decision or
  the end of the run
- `snapshot(const Game&, ...)`: host only, see below

Device C++: the code under `core/` must compile both as host C++17 and inside CUDA kernels.
- every function carries `SPIRE_FN` (`__host__ __device__` under CUDA, empty otherwise)
- no heap, no exceptions, no RTTI, no virtual functions, no recursion, no function pointers
- no standard library beyond `<cstdint>`, `<cstddef>` and fixed-size helpers defined in `core/`
- no mutable globals or statics; constant tables are `constexpr` arrays. The sole exception is
  the per-thread fault-pointer array in CUDA block shared memory in `platform.hpp`; it routes
  assertions to the owning `Game` and stores no rule state
- capacities and their Java basis are documented in `porting.md`. Java growing lists have
  execution budgets, not invented game limits; overflow is `SPIRE_ASSERT`, never truncation
- host assertions abort, including release builds. Device assertions latch the first call-site
  code in `Game::fault` and end only that thread with PTX `exit`. `step` on a faulted game is a
  no-op; the trainer reads faults through the batch API and resets those environments

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

Schema version **2** is defined here and in `inventory/snapshot-fields.json`. Java names in
that inventory are data identifiers, not copied implementation. `oracle/build.py` regenerates
it from the pinned JAR and `Snapshot.included`; the recorder loads the resulting catalog from
its own mod. The inventory root maps Java class names to `{extends, fields}` and has an
`excluded` section mapping each removed `package.Class.field` to its reason. Exclusion changes
the observation only, never the original field or update method. The engine must use the same field membership, including private subclass fields.
No value is rounded or omitted because it is zero, false, empty or null.

```typescript
type Action =
  | {type: "play", card: number, target: number}
  | {type: "end_turn"}
  | {type: "potion_use", slot: number, target: number}
  | {type: "potion_discard", slot: number}
  | {type: "choose", option: number}
  | {type: "proceed"}
  | {type: "skip"};
type CardSpec = {id: string, upgrades: number};
type Scenario = {
  deck: CardSpec[], relics: string[], potions: (string | null)[],
  hp: number, max_hp: number, encounter: string,
  profile?: {neow_spirits: number, bosses_seen: string[], seeded: boolean},
  playtime_at_least_800?: boolean,
  actions?: Action[],
  start?: {
    act: 1 | 2 | 3 | 4, floor: number, node: [x: number, y: number],
    room: "MonsterRoom" | "MonsterRoomElite" | "MonsterRoomBoss" | "RestRoom" |
          "ShopRoom" | "TreasureRoom" | "TreasureRoomBoss" | "EventRoom" | "NeowRoom",
    gold: number, keys: [ruby: boolean, emerald: boolean, sapphire: boolean],
    emerald: boolean
  }
};
type Header = {
  schema: 2, game_sha256: string, seed: number,
  character: "THE_SILENT", ascension: 20, scenario: Scenario | null,
  playtime_at_least_800: boolean
};
type Ref = {ref: number};
type Value = null | boolean | number | string | Ref | Value[] | {[key: string]: Value};
type ObjectState = {class: string, fields: {[declaringClassAndField: string]: Value}};
type RngState = [counter: number, state0: number, state1: number] | null;
type Position = [x: number, y: number] | null;
type MapNode = {
  position: Position, room: string | null, emerald: boolean,
  taken: boolean, edges: [destinationX: number, destinationY: number][]
};
type Snapshot = {
  run: RunState,
  player: Ref, room: Ref, map: MapNode[][], node: Position, next_node: Position,
  manager: Ref,
  combat: {
    totalDiscardedThisTurn: number, damageReceivedThisTurn: number,
    damageReceivedThisCombat: number, hpLossThisCombat: number,
    playerHpLastTurn: number, energyGainedThisCombat: number, turn: number, energy: number
  },
  screen: {
    type: string, up: boolean, previous: string | null, choice: string, options: string[], state: Ref | null,
    room_dialog: {optionList: Ref[], selectedOption: number, waitForInput: boolean},
    image_dialog: {selectedOption: number, waitForInput: boolean}
  },
  globals: Globals,
  pools: {[pool: string]: [cardId: string, upgrades: number, misc: number][]},
  rng: {
    monsterRng: RngState, mapRng: RngState, eventRng: RngState, merchantRng: RngState,
    cardRng: RngState, treasureRng: RngState, relicRng: RngState, potionRng: RngState,
    monsterHpRng: RngState, aiRng: RngState, shuffleRng: RngState,
    cardRandomRng: RngState, miscRng: RngState, neow: RngState, unseeded: RngState
  },
  objects: ObjectState[]
};
type Decision = {snapshot: Snapshot, legal: Action[], action: Action};
type Terminal = {snapshot: Snapshot, terminal: "death" | "victory" | "combat_end"};
```

All indices are zero based. A target of -1 means no enemy target. `seed`, RNG state words and
counters are signed Java integer values serialized as JSON integers; consumers must preserve
64-bit integers (do not decode them through a JavaScript `Number`). Float fields are the exact
Java float32 values serialized by Gson, compared numerically without tolerance. Enum values
are Java enum names. Strings use English localization with formatting retained where the game
stores it. Object member order is not semantic; array order is semantic throughout.

`objects` is an identity graph, not copies of pile entries. Traverse top-level fields in the
order above; within an object traverse catalog fields by lexicographic
`DeclaringSimpleClass.field` order. On first encounter reserve the next object index, then
visit its fields depth first. Every later encounter emits the same `Ref`. The class name is
relative to `com.megacrit.cardcrawl.`; the catalog's `extends` chain defines inherited fields.
A field key uses its declaring binary class name without the package (including `$` for inner
classes), so both `AbstractEvent.screenNum` and `NeowEvent.screenNum`
can exist. Captured fields of anonymous actions retain their owner references. Only fields named
in the catalog are emitted. Java arrays and lists preserve order;
Java maps have stringified keys sorted lexicographically. Card UUIDs are replaced by sequential
integers on first UUID encounter *within that snapshot*, preserving equality between copies.
Object references and UUID equality are distinct relationships. A replayer resolves references
from the roots and compares the resulting graphs; table indices are serialization labels, not
game state. It maintains forward and reverse identity maps, including across roots, so shared
objects cannot become copies and distinct objects cannot collapse into one. UUID labels use a
separate bijection that preserves equality between card copies. There are no adapter-owned
game objects or queue markers.

Pile `CardGroup.group` order is Java list order: index 0 is the bottom, the last element is the
top for draw/discard/exhaust groups; hand indices are its list indices. Master deck, relics,
powers, monsters, moves, card queues, pre-turn queues, pending actions and played-card histories
retain their original order. Card fields include costs, damage/block/magic values, upgrade and
bottle flags, retain/exhaust/autoplay flags, misc, multiDamage, rule-read flip state and subclass
state. Card text-color flags, preview copies, cursor hover targets, timers, action durations,
animation and sound state are excluded. `AbstractRelic.grayscale` remains: Captain's Wheel and
Horn Cleat read it. Creature `drawX`/`flipHorizontal`, slime `saveX`, and Hexaghost's attack-cycle
index remain because spawn ordering, back attacks, splitting and move selection read them.
For an escaping or escaped monster, `drawX` is projected to null: `updateEscapeAnimation` moves
it on every frame, and no encounter combines an escaper with a `SpawnMonsterAction` user, so
no subsequent rule reads that position. The conditional exclusion is recorded in the catalog.
Power, relic and monster fields include private subclass counters/references as well as
base state. `manager.currentAction` and all rule actions in queues are recorded even at interrupted
card-selection decisions. The observer omits exact `WaitAction` queue entries and projects an
exact `WaitAction` current action to null: its update only advances a presentation timer and has
no rule callback. Other action classes and their included fields remain in the graph. Graph
references retain the identity of their cards, powers and creatures.

`RunState` has the following exact keys (Java `AbstractDungeon` names except the final two):

- Strings: `id`, `bossKey`, `lastCombatMetricKey` (nullable where Java stores null).
- Integers: `floorNum`, `actNum`, `ascensionLevel`, `bossCount`, `cardBlizzRandomizer`,
  `cardBlizzStartOffset`, `cardBlizzGrowth`, `cardBlizzMaxOffset`, `smallChestChance`,
  `mediumChestChance`, `largeChestChance`, `commonRelicChance`, `uncommonRelicChance`, `rareRelicChance`.
- Booleans: `isAscensionMode`, `firstRoomChosen`, `isDungeonBeaten`, `is_victory`, `ascensionCheck`.
- Floats: `shrineChance`, `cardUpgradedChance`, `colorlessRareChance`, `shopRoomChance`,
  `restRoomChance`, `eventRoomChance`, `eliteRoomChance`, `treasureRoomChance`.
- Integer lists: `pathX`, `pathY`.
- String lists: `monsterList`, `eliteMonsterList`, `bossList`, `eventList`, `shrineList`,
  `specialOneTimeEventList`, `commonRelicPool`, `uncommonRelicPool`, `rareRelicPool`,
  `shopRelicPool`, `bossRelicPool`, `relicsToRemoveOnStart`.
- `seed`: signed 64-bit integer. `keys`: `[ruby, emerald, sapphire]` booleans.

`Globals` has the following exact keys and values:

- `event_chances`: `{ELITE_CHANCE, MONSTER_CHANCE, SHOP_CHANCE, TREASURE_CHANCE}` (floats).
- `potion`: `{blizzardPotionMod}` (integer).
- `shop`: `{purgeCost, actualPurgeCost}` (integers).
- `drawn_cards`, `scraped_cards`: ordered arrays of card references.
- `obtained_cards`: map from card id to acquisition count.
- `actions.common.DiscardAction`: `{numDiscarded}`.
- `actions.common.ExhaustAction`: `{numExhausted}`.
- `actions.common.PutOnBottomOfDeckAction`, `actions.common.PutOnDeckAction`,
  `actions.unique.CodexAction`: each `{numPlaced}`.
- `actions.unique.NightmareAction`: `{numDiscarded}`.
- `events.shrines.FaceTrader`: `{goldReward, damage}`.
- `powers.TheBombPower`: `{bombIdOffset}`.
- `relics.CentennialPuzzle`: `{usedThisCombat}` (boolean).
- `relics.OrangePellets`: `{SKILL, POWER, ATTACK}` (booleans).

The exact pool keys are `colorlessCardPool`, `curseCardPool`, `commonCardPool`, `uncommonCardPool`,
`rareCardPool` and the corresponding five `src`-prefixed names with capitalized first letters.
These library-derived template pools contain `[id, timesUpgraded, misc]` entries in pool order.
`screen.state` points to the active GRID, HAND_SELECT, CARD_REWARD, COMBAT_REWARD, BOSS_REWARD or
SHOP object, otherwise null; an event's private state is under `room.event`. `screen.options`
are ordered identifiers produced from vanilla objects: card/relic/potion IDs, reward-type enum
names, `option:<button slot>` for event buttons, `x=<column>`/`boss` for map nodes, campfire
option class names, and `open`, `shop`, `purge`, `bowl`, `spin` for those controls.
`choose.option` indexes this array. GRID `hoveredCard` is a pending confirmation target, so it
is included during confirmation and projected to null otherwise; its preview card and refresh
timer are excluded. All other values in catalog fields are the original game values.
Map rows, columns and destination edges retain Java order. Historical rooms on the map are
represented by their room type; the live room is the separate `room` graph root.

Legal actions are enumerated in this order: playable hand cards (hand index, then living enemy
index for targeted cards), end turn, occupied potion slots (uses by target index, then discard),
choices by option index, proceed, skip. Card/potion availability comes from the original
`canUse`/`canDiscard` methods; dead, escaping and half-dead enemies are excluded as player targets.
The policy is a separate libGDX `RandomXS128(seed)`, drawing `nextInt(legal.size())` exactly once
per decision; it never consumes a game RNG. A full run begins at the first Neow prompt and ends
on death or the victory screen. A scenario starts at the first combat input after the specified
setup and ends on death or combat completion, after original room updates have settled.
The initial profile sets `THE_SILENT_SPIRITS=1`, giving Neow's four-reward opening; fresh isolated
preferences retain the game's default normal animation speed (`FAST_MODE=false`).

Scenario setup starts a fresh A20 Silent run, replaces the master deck and ordered relics,
applies original relic `onEquip` hooks, fills potion slots (null means empty), then assigns HP.
Relics are acquired one at a time in input order. Their acquisition screens and callbacks run
to completion before acquiring the next relic. Setup choices use an independent
`RandomXS128(seed)` and the same uniform legal-action enumeration. These setup choices precede
the trace; the combat policy starts with its own `RandomXS128(seed)`. The input deck is the deck
before acquisition: original acquisition effects can upgrade, remove or transform its cards.
It enters a plain Act 1 `MonsterRoom` at floor 1 using the original `nextRoomTransition` and the
specified encounter as its next monster-list entry. This defines the scenario context; it does
not apply burning-elite modifiers or boss-room rewards. Piles, energy, monsters and relic battle
hooks are initialized by the game.

The recorder's run-level scenarios supply `start`. After fresh Neow initialization, setup assigns
the requested keys and the floor before room entry, then constructs the requested later dungeon
through its original constructor (Act 1 retains its existing generated dungeon). The departing
room is disposed before replacing it with an EmptyRoom, following `setCurrMapNode` and clearing
the opening event's dialogs. Gold is assigned before the deck/relic/potion/HP setup above. Setup
replaces the requested generated node's room, sets its emerald flag and the chosen-node/path
state, overrides the corresponding monster/elite entry or boss key for combat rooms, and calls
the original `nextRoomTransition`. The requested floor is the floor after that transition.
These recordings continue through rooms and acts until death or victory; combat-only scenarios
without `start` retain their existing combat-end boundary. The engine applies the same setup.
A `NeowRoom` start replaces the opening room after acquisition and HP setup, without a floor
transition. Optional `profile` supplies the original Silent spirit count and boss-seen preference
keys and the original `Settings.seedSet` before dungeon construction; its default is one spirit,
no seen bosses and a manually seeded run. Mini blessings require `seeded: false`, because
`NeowEvent` ignores spirit preferences in manually seeded runs. This covers
mini blessings, first-unseen boss selection and the all-seen shuffle without changing the policy.

The headless platform uses 1280x720, a fixed float32 1/60 tick, no input devices and no GL/audio
output. Original update/render callbacks execute. Decisions wait for Soul settlement, monster
death/intent settlement, card-manipulation and `FastCardObtainEffect` callbacks,
`ObtainPotionEffect`, `GainPennyEffect`, turn/battle-start callbacks, key acquisition,
relic movement to its slot and the resulting `onEquip`,
and campfire callbacks unless a selection needs input. No oracle object enters any game queue.
`AbstractPlayer.playCard`, original screen update/selection methods and original button handlers
apply inputs. Combat input requires `WAITING_ON_USER`, empty action/pre-turn/card/monster queues,
no current action, no ended/ending/queued turn, and the original end-turn button enabled.
Outside combat, pending actions also block a decision. The sole exception is an original
GRID/HAND_SELECT/CARD_REWARD input blocking the current action; that action and its queues are
recorded. A completed current-action object is not pending. Terminal death can preserve queues
that the dead game's own update loop no longer executes. Map transitions, automatic hand-select
closure, event-dialog entrance/pressed-button callbacks, consumption of closed GRID selections,
and delayed room controls finish before the next decision. Timers are not inputs to the
engine: queue location and the active selection encode the action's semantic stage.
Card, relic and ascension unlock queries are fixed to unlocked, and Act 4 availability is true.
The runtime loads ModTheSpire plus `Oracle.jar` only. BaseMod and CommunicationMod are absent.
The launcher disables ModTheSpire's bundled game patches and loads Oracle without Workshop discovery. The publisher adapter uses the game's no-op GOG implementation;
it never initializes Steam, and metrics upload is disabled.
The exact runtime game-class patch allowlist and reasons are in `inventory/runtime-patches.json`.
A read-only Java instrumentation agent in the launcher compares all 2,306 game classes as they
load with their bytes in the original JAR; every recording checks the complete changed-class set.
No game bytecode is written to the repository. Rendering/device state and persistent
profile/score/unlock metrics are outside the snapshot.
Gameplay-affecting unseeded calls are isolated as specified in `inventory/unseeded.md`. The
presentation RNG is separate and cannot supply the listed rule calls. Batches reset rule statics
to their fresh-process values before each seed; this includes The Bomb's id allocator, turn/energy
counters, dialog selections, stale next-room/encounter references, and the captured cold-menu
relic exclusion list. Device-cache RNG is isolated from both presentation and rule streams.
The grid screen is recreated between runs; its transform timer remains original game state but
is outside the rule snapshot. Transform-preview randomness uses the presentation stream,
including the otherwise implicit default-Random constructor draws.

Each UTF-8 JSONL file is `Header`, followed by zero or more `Decision` records, then one `Terminal`.
Every line ends with LF. Recorded data is never edited. Committed fixtures may use `.jsonl.gz`:
gzip is only a lossless storage envelope; replay compares the decompressed JSONL unchanged.

## Tests
`tests/` replays a trace through the engine: `reset` (or apply scenario setup), then for each line
compare both scopes and legal actions before `step` with the recorded action. Every included
field belongs to a scope; comparisons within each scope are exact, including types, array order,
reference identity and all RNG state words. Root traversal avoids unrelated object-table index
shifts. Identity maps are shared between the two scope comparisons at each snapshot.

- **Combat:** the complete `player` graph, `room.AbstractRoom.monsters` graph, complete `manager`
  graph, the `combat` block, legal actions, and `aiRng`, `shuffleRng`, `cardRandomRng`,
  `monsterHpRng`, `miscRng`, `unseeded`. Screen fields belong here except the dialog objects and
  reward/shop state described below. This includes `type`, `up`, `previous`, `choice`, `options`
  and the complete active selection graph (`HAND_SELECT`, `GRID`, `CARD_REWARD`). No card,
  relic, potion, power, monster or action field inside these subtrees is filtered out.
- **Run:** all other snapshot roots and RNG streams, the room graph excluding only its monster
  field, `screen.room_dialog`, `screen.image_dialog`, and `screen.state` when the screen type is
  `COMBAT_REWARD`, `BOSS_REWARD` or `SHOP`. In particular, reward contents are run state while
  the screen's type and available option identifiers remain combat state at `combat_end`.
  `globals` belongs to run scope even when it contains references to combat cards; reference
  identity with the combat graph is checked. New top-level roots belong to run scope until the
  contract explicitly assigns them elsewhere; new fields under a combat root remain combat.

Both scopes are checked at every decision and terminal record even after earlier differences.
A recorded action is applied only when it is legal in the engine; otherwise the trace reports
where replay stopped. A scenario passes when both scopes match at every decision and the terminal snapshot/reason
matches. Run differences never hide a combat result. The committed `full-run-1` fixture has the same acceptance requirement as every other trace. Each trace prints both scope results and its
first difference per scope, including the JSON path and both values. Summary counts report
passing traces and matching decision records per scope; terminal snapshots affect trace success
but are not counted as decisions. Matching later decisions are counted after an earlier failure.

Observation omissions belong to the oracle implementation: correct the observation code and
field catalog, rebuild and record fresh traces. Never hand-edit records. Changes to audited game
patches or the rule/presentation RNG split require an explicit contract change, and suspected
problems there are reported while independent porting continues.

## Oracle (`oracle/`)
Drives the original game logic headless and writes traces; deterministic environment patches are listed in the unseeded inventory. Two kinds:
- scenario traces: a chosen deck, relics, potions and encounter, to cover each unit in isolation
- full-run traces: whole games from a seed under a seeded random legal policy

## Inventory (`inventory/`)
One row per unit in scope (card, relic, potion, power, monster, encounter, event, room/screen
rule, ascension modifier) with its Java path. Status is computed from test results, never typed
by hand.

Build the local oracle with `python3 oracle/build.py`. This links locally installed JARs/runtime
into ignored `oracle/instance/`, compiles only our adapter, and writes the field catalog.
Record full runs with `python3 oracle/record.py --seed 1 --count 20 --output /tmp/oracle-full`;
add `--scenario tests/scenarios/starter.json` for combat scenarios. `--workers 1|2|3` partitions
seeds between isolated JVMs. Each process owns its working directory, preferences and saves;
no service or network transport is started. `python3 oracle/verify.py --output /tmp/oracle-check`
records two concurrent batches in separate JVMs, reversing job order in the second batch, and compares
20 full runs plus 200 scenarios byte for byte. Timings exclude JVM startup but include game reset,
original callbacks, snapshot construction and JSONL writes.
Regenerate the committed fixtures from their recorded headers with
`python3 tests/record_oracle_fixtures.py --output /tmp/oracle-fixtures`, then check their schema,
scenario initialization and terminal records with `python3 tests/check_oracle.py`.

`python3 tests/check_oracle_frames.py` compares full runs on seeds 1 through 31, every committed
scenario and the run-start scenarios against the same recordings with 13 extra original game
frames before every observation, including terminal snapshots. The comparison is byte for byte.
Its test agent changes the recorder only, not any game class; it also checks transform-preview
confirmation and the terminal observation of escaped monsters.
