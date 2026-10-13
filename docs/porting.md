# Porting a content unit

The original Java source is the behavior specification. Read the constructor and every overridden
method of the unit, then follow the actions and hooks those methods call. Do not infer behavior
from a wiki, the displayed description, or another simulator.

## Files owned by one unit

A catalog row names the enum entry, Java ID, header path from the repository root, and C++ type:

```cpp
SPIRE_UNIT(Neutralize, "Neutralize", "core/cards/neutralize.hpp", Neutralize)
```

To add a unit that uses existing engine mechanisms, add its header and a row to that category's
`catalog.def`. Append the row without reordering existing IDs. Add its focused trace or test and
inventory entry. Do not edit the engine's dispatch, a shared list of IDs, or host names.

CMake runs `host/generate_content.py` before compilation. It writes enum IDs, base hooks,
declarations and switch dispatch into `spire_generated/` inside the build directory. Host names
read the same catalogs. Card metadata is a shared constexpr table built from each unit's
`data()`; `card_def` returns a const reference and adds no per-Game storage. Generated files are build products; do not edit or commit them.

## Unit code

Include `../hooks.hpp`, put the type in `spire::units::<category>`, and inherit the category's
static hook base. For example, `Neutralize` inherits `CardHooks` in `spire::units::cards`.
Every function, including `data()`, carries `SPIRE_FN`.

| Category | Base and metadata | Unit entry points |
| --- | --- | --- |
| Cards | `CardHooks`, `CardDef` | `use(Game&, int card, int target)`, `upgrade(Card&)` |
| Powers | `PowerHooks`, `PowerData` | Java overrides such as `atStartOfTurn(Game&, Power&, int owner)` and `atDamageGive(const Game&, const Power&, int owner, float value, DamageType, int card)` |
| Relics | `RelicHooks`, `RelicData` | Java overrides such as `atBattleStart(Game&, int relic)` |
| Potions | `PotionHooks`, `PotionData` | `use(Game&, int slot, int target)`; metadata gives potency, target and usability |
| Monsters | `MonsterHooks`, `MonsterData` | `construct(Game&, int hp_override)`, `getMove(Game&, int owner, int roll)`, `takeTurn(Game&, int owner)` |
| Encounters | No base or metadata type | `build(Game&)` |
| Actions | `ActionHooks`, `ActionData` | `prepare(Game&, Effect&)` for constructor behavior, `execute(Game&, Effect&)` for queued behavior |

Monster and encounter headers include their local `base.hpp` for construction and encounter
helpers. Other unit headers include `../hooks.hpp`. `core/api.hpp` and the generator's hook
registry give the exact signatures of additional hooks.

`data()` returns the category's constant metadata. Card metadata contains type, target, cost,
base damage/block/magic, upgrade deltas and exhaust/ethereal flags. Keep the values beside the
unit's behavior. A card's `use(Game&, int card, int target)` queues its effects; the engine owns
card movement, energy payment and the `UseCard` action. `upgrade(Card&)` mutates the card
instance; it does not require a temporary `Game`.

Implement the Java hooks the unit overrides. The static base supplies inherited defaults. Action metadata declares whether post-combat cleanup retains the action. A
generated declaration is not evidence that its engine call site or complete Java behavior has
been ported; verify both before relying on it. Do not add empty overrides for hooks the Java
unit inherits. An empty method that the Java unit overrides, such as `Slimed.use`, has a
source-defined meaning and can stay empty.

Use the declarations in `core/api.hpp` for shared engine operations. Queue actions in the order
used by Java. A unit must not skip the queue to perform an operation Java schedules for later.
Use `queue_card_damage` for an already calculated card hit and `queue_damage` for a base monster hit. `add_top`/`add_bottom` run action construction before insertion; ApplyPower stores the constructed power separately from the raw stack amount. Preserve ordered power/relic/card iteration, RNG stream selection and RNG call counts. For a
surprising Java behavior, cite the class and method in the code comment.

The unit header must remain device C++17: no allocation, exceptions, RTTI, virtual methods,
function pointers, recursion or mutable static state. Use fixed-capacity engine containers;
overflow must assert. Host-only helpers and Python objects stay outside `core/`.

## State and new engine mechanisms

A unit's state belongs to its card, power, monster, relic, potion or queued action instance inside
`Game`. Do not place instance state in a global variable, a singleton or a shared side table.
A future hook must observe the same object instance and state changes as Java. Declare fields
inside the unit type, using that type's name:

```cpp
SPIRE_STATE(MyPower, int counter; bool active;)
```

The generator places these fields in the category's tagged union; a power hook accesses them
through `power.state.MyPower.counter`. Fields must have trivial default construction: scalars, enums, C arrays or compact
value records such as `DamageArray`, without field initializers or `List` members.
Non-scalar records require an explicit `SPIRE_SNAPSHOT` mapping. Set constructor values after initializing the
selected union member. This keeps state local to each instance and adds storage for the largest
payload in that category rather than every unit's payload.

| Category | State owner | Generated initialization |
| --- | --- | --- |
| Cards | `card.state.Type` | `card_initialize_state(card)` |
| Powers | `power.state.Type` | `power_initialize_state(power)` |
| Relics | `relic.state.Type` | `relic_initialize_state(relic)` |
| Potions | `game.run.potion_state[slot].Type` | `potion_initialize_state(game, slot)` |
| Monsters | `monster.state.Type` | `monster_initialize_state(monster)` |
| Encounters | `game.encounter_state.Type` | `encounter_initialize_state(game)` |
| Actions | `effect.payload.state.Type` | `action_initialize_state(effect)` |

Snapshot mappings for reference fields and Java names live beside the unit's state:

```cpp
SPIRE_SNAPSHOT("ConstrictedPower.source", creature, power.state.Constricted.source)
SPIRE_SNAPSHOT("DrawCardAction.followUpAction", action_reference, effect.payload.state.Draw.follow_up_action - 1)
```

These declarations contain field identifiers and value expressions; they emit no device code.
The host generator emits dispatch to the graph's scalar, card, creature, power and action
reference writers. Scalar state fields with matching Java names use the generated state visitor.
Run-owned Java statics use `SPIRE_STATIC_STATE(Type, fields)` beside their unit; the generator
places them in `Game::unit_statics`. They survive unequipping the unit and remain isolated
between Game instances.
`SPIRE_WAITING_RULE("Java caller: missing rule")` records a rule dependency beside its unit;
`tests/content.py` includes it in generated status without adding a caller.
An explicit mapping overrides that visitor and inherited base fields. `SPIRE_STATIC("field", game.expression)` maps a unit static; the inventory generator reads observed static initial values from Java. Power flags and action
categories belong in `data()`. Adding a unit does not change the serializer.

Set the instance ID before calling its initializer. Existing card, power, relic and monster
construction paths and combat setup initialize their selected payload. A new action constructor
must initialize its effect before assigning payload fields and queueing it. Snapshot field names,
membership and object identity follow `inventory/snapshot-fields.json` and `docs/design.md`;
the engine's internal field names do not define the trace format. Preserve included Java fields,
including ordered histories. The catalog's `excluded` entries are outside the contract: do not
add timers, animation queues or other presentation state to match them. Creature positions used
for spawn ordering and other rule-read fields remain part of the engine.

## Device memory budget

The integration target is **32 KiB per Game**. `state.hpp` enforces a **48 KiB hard ceiling**
with `static_assert`, leaving space for the remaining content. Do not raise this ceiling to
land a unit. Every build of a core consumer runs `host/check_layout.py`: Clang discovers the
fields and prints every `Game` member, its element size/capacity, the element types and their
fields, and every generated unit payload. There is no handwritten member list to keep in sync.

The generator also enforces the largest payload in each category: cards, powers, relics and
potions 8 bytes each; monsters 32; encounters 16; actions 48. Current maxima are cards 1,
powers 4, relics 4, potions 4, monsters 8 and actions 16 bytes. A field added to one union member
is paid for by every object of that category. Read the build breakdown before adding state.
For a larger payload, put bounded storage in a shared Game-owned arena and keep an index in the
unit state; follow the existing action, power, text and multi-damage stores. Choose a capacity
from the Java lifetime and coexistence of those objects, document where an unbounded Java list
becomes an execution budget, and test overflow through the fault mechanism below. Keep scalar
state with its unit, constructor facts in unit data, and derivable observation fields on the host.
Do not store a host pointer, vector, string, whole card or large per-unit array in a payload.

## Storage and capacities

`Game` owns its storage and remains relocatable by a byte copy. Keep object identity separate
from an object's current position in an ordered list:

- Dungeon maps store 15 by 7 two-byte nodes: a destination-column bitset plus room kind,
  emerald and taken bits. Edges are enumerated in ascending column order; row 14 points to
  boss row 16. The Ending uses five rows with its original rest/shop/elite/boss/victory layout.
  Generation uses temporary six-entry ordered parent lists, including duplicate parents:
  `MapGenerator.createPaths` creates six paths and `addParent` does not deduplicate them.
  Those parent lists are not retained in `Game`. Monster/elite/boss lists reserve 16/10/3
  byte IDs, the maxima produced by the four dungeon generators. Event/shrine/one-time
  lists reserve 16/8/16 byte inventory keys and retain Java order.
- Master-deck cards removed while a relic references them stay in the existing 160-card arena.
  A 64-byte index list covers the 64 relic instances; these retained master objects consume
  the same execution budget as combat cards and keep a distinct snapshot identity.
- Master-deck and combat cards are different objects with matching UUIDs
  (`AbstractCard.makeSameInstanceOf`). The master deck holds 128 cards and the combat arena holds 160; piles and played histories
  hold byte indices into combat cards. The Reptomancer high-health recordings exceed 128
  combat card objects through Wound generation. These capacities are execution budgets.
  Do not put `Card` values in another pile or queue.
- Each creature holds the head and size of an ordered list in the shared 128-object power arena.
  `Power::next` links the objects with byte indices; the per-creature budget remains 32. Use
  `creature.powers.at(game.power_objects, index)` and pass `game` to power queries. Removing a
  power removes its list entry, retaining the object until combat reset so queued Java object
  references still observe it. These 128 slots include detached powers, not just active powers.
- Prepared powers and other action state share the tagged `EffectPayload` union; access action
  state through `effect.payload.state.Type`. A queued ApplyPower owns its prepared Power until
  execution, preserving constructor-time fields and identity.
- The 64 action slots are shared by the current action, both queues and held follow-ups.
  Queues hold byte indices. Completed ordinary actions release their slot. Clear queues with
  `clear_actions(game, queue)` to release discarded slots as well. `hold_action` pins
  an object until combat reset; queue its index to preserve identity. Java
  `GameActionManager.update` skips an already completed action, including a reused follow-up.
- The monster arena retains 64 objects, including dead and escaped monsters. Java
  `SpawnMonsterAction.update` inserts without removing earlier objects. Reptomancer recordings
  retain 39 objects; the high-health integration scenario retains 63. This is an execution
  budget, not a Java maximum. Never recycle a dead monster's identity.
- Monster move histories share 1,024 nodes, each holding the signed byte move and a previous
  10-bit previous-node index. The full 1,024-node arena uses 2,306 bytes. Each monster keeps a head and length; both rule lookbacks and host snapshots
  read the same history. The history budget spans the entire combat.
- Each monster keeps three base/output damage pairs. Additional pairs use a shared 16-entry
  arena: GiantHead constructs eight DamageInfo objects, Transient seven, Guardian and
  WrithingMass four. Summoned daggers use three and TorchHead one; retaining these monsters
  does not consume extra damage slots. Use `monster_base_damage` / `monster_output_damage`.
- `game.multi_damage` stores a 32-bit common value per card and an index into the shared
  255-entry `damage_exceptions` arena. Actions use the same exception arena; completed or
  discarded actions release their entries through their `release` hook.
  The current encounters have at most five living monsters; dead targets receive the common
  untargeted card damage in `AbstractCard.calculateCardDamage`. Dead monster indices remain in
  the array, and its original length stays in the card or action. `DamageAllEnemiesAction`
  shares a card's row when Java shares its array. Rows and the exception arena reset at combat start. The arena is an execution budget;
  overflow faults instead of truncating arrays.
- `manager.played_cards` stores each play once, including repeated references to the same card.
  Combat history is `[0, size)`; turn history is `[turn_start, size)`. Compute type counts from
  these references as `DamagePerAttackPlayedAction.update` does, without duplicate counters.
- Text needed by action state uses offsets into a 1,024-byte per-combat interned buffer, with
  offset zero representing Java null. No process pointers or per-action character buffers.
  Constant facts and derivable snapshot fields belong in unit data and host projections.
- Pending card acquisition effects hold byte indices in a 160-entry list into the same card
  arena. Their callbacks settle before a decision; they do not enter the Java action queue.

The hand limit of 10 comes from `DrawCardAction.update` and card-generation actions; Armaments'
`cannotUpgrade` list only collects that hand (`ArmamentsAction.update`). Source card pool
indices use bytes: the current Silent pools contain 19 common, 33 uncommon, 19 rare and 35
colorless templates. Each source pool reserves 64 entries and preserves Java list order,
including the remove-and-append behavior of `AbstractDungeon.srcTransformCard`.

Relic acquisition shares a five-potion reward list (`Cauldron.onEquip` creates five),
the 16-entry room-reward list (`CallingBell.update` adds three relic rewards), and the existing
selection piles. The five ordered relic pools each reserve 64 byte keys; the pinned Silent
library has fewer than 64 entries in each tier. The keys name inventory content, including
content awaiting later batches; dispatch requires the corresponding catalog unit. Card-reward
generation uses a 128-card temporary list, an execution budget for Java
`AbstractDungeon.getRewardCards`, rather than adding card objects to Game.

The remaining budgets are 32 card-queue entries, 64 relics and 1,024 played-card references
per combat. The master deck starts with 13 cards at
A20 (`TheSilent.getStartingDeck` plus the ascension curse); its 128 slots leave room for rewards
and events. The combat arena has 160 slots including copies, templates and created cards.
**These capacities are execution budgets, not Java limits.**
Java uses growing lists; card-creation loops and repeated turns have no finite bound. Histories,
held actions, detached powers, text and damage rows can also exhaust their budgets. A valid Java
sequence can therefore fault an environment; count this separately from losses and reset it.
Never silently truncate, overwrite live slots, or shrink an amount to fit a capacity.

Card damage/block, power amounts and player energy keep Java's 32-bit range. The 133
standard cards have at most one upgrade; their upgrade counts use one bit. Card UUIDs use
16 bits: combat setup renumbers the master deck, and subsequent UUID creation consumes the
160-card arena (including retained acquisition objects). Rules that extend that lifetime must
preserve this bound or widen the identity. Bottled-card references use the same width.
Base block derives from card metadata and upgrade count. RitualDagger keeps its Java
`applyPowers` baseBlock/misc behavior in its own state; stat-equivalent copies preserve this
state. Do not introduce mutable base block without updating this representation. Magic numbers
come from constructor/upgrade constants (at most 60) or Regret's hand count, so magic fields
use signed bytes. Content with different ranges must widen these fields before porting it.
Poison's constructor cap, for example, is not its stacking limit. Catalog IDs and bounded indices use
bytes; signed card references and costs retain 16 bits, and monster moves use Java signed bytes.
The index-width assertions in `types.hpp` must hold when changing capacities.

## Device faults

`SPIRE_ASSERT` is the only capacity/invariant failure mechanism. Host builds trap even with
`NDEBUG`. On device, the first failure records a nonzero 32-bit hash of `__FILE__` and `__LINE__`
in `Game::fault`, then [PTX `exit`](https://docs.nvidia.com/cuda/parallel-thread-execution/#control-flow-instructions-exit)
ends that thread before the failed operation writes out of bounds. The compiler memory clobber
keeps the fault store before the exit. Earlier mutations in that step are not rolled back.
`step` then does nothing until `reset` clears the fault; `legal_actions` returns an empty set.
Fault metadata is outside the oracle snapshot and does not filter the comparison.

The block-shared pointer array in `platform.hpp` binds each CUDA thread to its own fault word.
This is the sole exception to the no-mutable-statics rule, not a content state mechanism.
`reset`, `start_combat` and `step` bind their Game on entry. Custom kernels must call
`bind_fault(game)` before other core operations that can assert, including `legal_actions`.
There are 1,024 pointer slots per block (8 KiB), CUDA's maximum threads per block; each lane
writes and reads only its own slot. No pointer is stored in `Game` or shared across launches.

Host batches expose `faults()` and `reset_at(index, seed)`; individual Python environments
expose the read-only `fault` property. CUDA batch entry points are declared in `cuda/batch.cuh`:
`spire_faults_batch` writes one fault code per environment, and `spire_reset_batch` resets the
listed environment indices with their supplied seeds. Launch these kernels with one-dimensional
grids/blocks and assign each environment to one thread. Reset indices must be distinct.

The normal build checks the host device-C++ restrictions and compiles the device branch and
batch kernels to NVPTX-targeted LLVM with Clang, without requiring a CUDA installation.
When CUDA is available, CTest adds `test_cuda_faults`: a capacity failure must leave the other
lane running, preserve the first fault, make later steps inert and allow an indexed reset.
That runtime test is skipped when there is no GPU; LLVM generation is not GPU execution.

## Hook call sites still absent

All gameplay hook names from the four Java base classes have generated declarations and static
dispatch. The following callers are not implemented; declarations alone do not establish parity.
Adding a queued action or content unit can call the existing dispatcher from its own header.

| Missing caller | Hooks awaiting that caller |
| --- | --- |
| Room entry, campfires, chests, rewards and shops | Relic `onEnterRoom`, `justEnteredRoom`, `onEnterRestRoom`, `addCampfireOption`, `canUseCampfireOption`, `onRest`, `onSmith`, `onChestOpen`, `onChestOpenAfter`, `changeNumberOfCardsInReward`, `changeRareCardRewardChance`, `changeUncommonCardRewardChance`, `canSpawn`, `getPrice` |
| Actions not in the current catalog | Power `onScry`, `triggerMarks`; card `triggerOnScry`, `onChoseThisOption` |
| Additional relic/monster rules and bosses | Relic `onTrigger`, `checkTrigger`; monster `deathReact`, `onBossVictoryLogic`, `onFinalBossVictoryLogic` |
| Stance and orb mechanics | Power `onChannel`, `onChangeStance`; relic `onChangeStance`; card `switchedStance`, `triggerExhaustedCardsOnStanceChange` |

Java hooks without gameplay callers (`onGainCharge`, `onMeditate`, `onRitual`, `onPlayerGainBlock`,
`onEvokeOrb`, `triggerAtStartOfTurn`, `escapeNext`, and the integer `onPlayerGainedBlock` overload)
have defaults but no invented events. Copy/upgrade/query methods are callable by their consumers.
## Checking a port

Add `tests/scenarios/<unit>.json` using the original Java IDs. All six keys are required:

```json
{
  "deck": [{"id": "Neutralize", "upgrades": 0}, {"id": "Strike_G", "upgrades": 1}],
  "relics": ["Ring of the Snake"],
  "potions": [null, null],
  "hp": 59,
  "max_hp": 66,
  "encounter": "Cultist"
}
```

For a power or queued action, choose cards, relics and monsters that trigger it. Scenario setup
starts a fresh Silent A20 run, replaces the deck, acquires relics in order through `onEquip`,
fills potions, assigns HP and enters a floor-1 MonsterRoom. The deck is the input before relic
acquisition effects. See `tests/scenarios/starter.json` for the starter deck.

For a run-level recording, add a `start` object to the same scenario. All fields in that object
are required; `node` names the generated map node whose room is replaced for the start:

```json
"start": {
  "act": 4, "floor": 52, "node": [3, 0], "room": "RestRoom",
  "gold": 2000, "keys": [true, true, true], "emerald": false
}
```

The recorder uses original dungeon constructors and room-entry callbacks and records through
death or victory. `encounter` overrides the corresponding list entry or boss key when the
starting room is a monster, elite or boss room. The existing combat-only format keeps its
original behavior. `docs/design.md` specifies the setup order. Engine support for replaying
these run-level starts is pending.

Record the original game with several seeds; at most three JVMs may run at once. The oracle
must have been built with `python3 oracle/build.py` before recording.

```sh
python3 oracle/record.py --seed 101 --count 3 --scenario tests/scenarios/Neutralize.json --output /tmp/spire-neutralize
```

Build, test and replay those files:

```sh
cmake --build /private/tmp/spire-build --parallel 1
ctest --test-dir /private/tmp/spire-build --output-on-failure
PYTHONPATH=/private/tmp/spire-build "$SPIRE_PYTHON" tests/replay.py /tmp/spire-neutralize
```

`SPIRE_PYTHON` and build configuration are described in the root README. The build checks source
and generated headers against the same device restrictions. Native and host tests establish
only their covered behavior. A combat unit is accepted when its original-game scenario traces have zero differences in both
scopes through the terminal record, including legal actions, every included player/card/monster/
action field, selection state and all RNG counters/state words. Full-run acceptance requires
both scopes as well.

Run all native tests, host tests and the committed combat fixtures with one command:

```sh
ctest --test-dir /private/tmp/spire-build --output-on-failure
```

The `combat_replay` test compares every committed scenario trace. Add `-V -R '^combat_replay$'`
to the CTest command to show per-trace results on success as well. The replayer requires schema 2
and accepts `.jsonl` and `.jsonl.gz` files and directories. Directory discovery includes scenario and full-run traces. Each trace prints separate `combat`
and `run` results, their matching/total decision counts, and the first differing JSON path with
both values in each failed scope. Both scopes are checked at every decision, including after a
difference when the recorded action remains legal. The final line counts passing traces and
matching decision records per scope. Terminal snapshots and reasons must match for a trace to
pass but do not add a decision to the count. Both scopes must pass for a scenario or full-run trace. The committed `full-run-1` fixture is
reported separately as an expected failure until the rules wave; it never counts as a pass.

Comparison resolves object references from roots and checks identity with bijective mappings;
renumbering the object table does not create a difference. Combat includes the whole player,
monster and manager subtrees, the combat block, screen controls/options, selection state and
`aiRng`, `shuffleRng`, `cardRandomRng`, `monsterHpRng`, `miscRng`, `unseeded`. Run includes all
other roots/streams, room reward state, dialogs and reward/shop screen state. Reward-screen
option identifiers remain combat state. See `docs/design.md` for the exact partition; do not
move a field between scopes to make a trace pass.

Keep generated batches outside the repository. Add a small representative fixture by lossless
compression, for example `gzip -n -c /tmp/spire-neutralize/101.jsonl > tests/traces/Neutralize-seed101.jsonl.gz`.
Keep the original header, scenario, actions and snapshots unchanged.

For the implemented content set, `tests/content.py record /tmp/spire-content` records every
scenario on seeds 101, 102 and 103 in one JVM. Each job restores the initial Boss-seen
preferences, so an earlier Boss encounter cannot change a later job's run initialization. After building the engine, run
`PYTHONPATH=/private/tmp/spire-build "$SPIRE_PYTHON" tests/content.py replay /tmp/spire-content`.
This generates `inventory/status.json` from strict replay results, including source/scenario/trace
hashes, per-scope counts, terminal results and first differences. Do not edit that file. A unit's
`matched` status requires its primary scenario and all runnable supplements to match; the separate
blocked count is not a pass. Encounter coverage comes from the scenario encounter; powers and
queued actions also report direct object observations. `unobserved` means no such evidence in
this recording set, not a replay failure. Rules-only hook consumers remain a separate list even
when the unit's combat scenario matches. This covers the recorded scenarios, not every interaction.

Use the Java class name for a primary scenario and `<Class>-<case>.json` for a supplement.
Scenarios waiting for content use `blocked--<kind>-<ID>--<Class>-<case>.json`, stripping spaces and
punctuation from the dependency ID in the filename; the JSON keeps the original Java IDs.
Multiple dependencies are joined with `+`. These scenarios are recorded too. Only absent catalog content in the scenario or actually acquired objects in its oracle
snapshots is classified as blocked; differences or assertion faults remain failures.
Use `blocked--rule-<caller>--<Class>-<case>.json` when an existing scenario still needs a rules
caller (for example a real Boss room). Its ambient replay may match without exercising that hook. The next wave can replay the existing recordings after
adding the dependency, then rename the scenario when it passes.

Never edit a recorded trace, filter comparison fields, relax types or add numeric tolerances to
make a port pass. An observation omission is fixed in `oracle/java/Snapshot.java` and the field
catalog by rebuilding with `python3 oracle/build.py`, then re-recording affected scenarios.
Keep the audited game patches and rule/presentation RNG split unchanged. Report any suspected
problem in those contracts with the JSON path and Java evidence while continuing other work.
If engine state is missing, implement its source behavior; do not substitute a trace value or
invent a default.
