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
read the same catalogs. Generated files are build products; do not edit or commit them.

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
| Actions | `ActionHooks`, `ActionData` | `prepare(Game&, Effect&)` for constructor behavior, `execute(Game&, const Effect&)` for queued behavior |

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
through `power.state.MyPower.counter`. Fields must be trivial scalars, enums or C arrays,
without field initializers or `List` members. Set constructor values after initializing the
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
| Actions | `effect.state.Type` | `action_initialize_state(effect)` |

Set the instance ID before calling its initializer. Existing card, power, relic and monster
construction paths and combat setup initialize their selected payload. A new action constructor
must initialize its effect before assigning payload fields and queueing it. Generated visitors
serialize these fields without a host edit. Combat objects contain a `state` dictionary;
`run.master_deck_state`, `run.relic_state` and `run.potion_state` follow their corresponding
ordered lists, and `combat.encounter_state` holds the encounter payload.

Monster and card-play histories retain a two-entry suffix plus total counts; card histories also retain
counts by card type. Snapshots expose the suffix and these counts. A rule that reads older
entries requires an explicit state design rather than pretending that the suffix is a full log.

The current budgets are 256 card instances, 8 monster instances, 64 powers per creature and
128 actions in each queue. They cover the current content and tested workloads. Card creation
and action queues have no finite Java-wide bound; these are asserted execution budgets, not
proof that every future deck or encounter fits. Card damage/block/magic and power amounts
remain 32-bit. Pile indices and container counts are 16-bit; monster moves are signed bytes.

## Hook call sites still absent

All gameplay hook names from the four Java base classes have generated declarations and static
dispatch. The following callers are not implemented; declarations alone do not establish parity.
Adding a queued action or content unit can call the existing dispatcher from its own header.

| Missing caller | Hooks awaiting that caller |
| --- | --- |
| Room entry, campfires, chests, rewards and shops | Relic `onEnterRoom`, `justEnteredRoom`, `onEnterRestRoom`, `addCampfireOption`, `canUseCampfireOption`, `onRest`, `onSmith`, `onChestOpen`, `onChestOpenAfter`, `changeNumberOfCardsInReward`, `changeRareCardRewardChance`, `changeUncommonCardRewardChance`, `canSpawn`, `getPrice` |
| Actions not in the current catalog | Power `onScry`, `triggerMarks`, `onDamageAllEnemies`; card `triggerOnScry`, `triggerOnGainEnergy`, `triggerWhenCopied`, `onChoseThisOption` |
| Additional relic/monster rules and bosses | Relic `onTrigger`, `checkTrigger`; monster `changeState`, `heal`, `deathReact`, `onBossVictoryLogic`, `onFinalBossVictoryLogic` |
| Stance and orb mechanics | Power `onChannel`, `onChangeStance`; relic `onChangeStance`; card `switchedStance`, `triggerExhaustedCardsOnStanceChange` |
| PlayerTurnEffect completion | Power `atEnergyGain` |

Java hooks without gameplay callers (`onGainCharge`, `onMeditate`, `onRitual`, `onPlayerGainBlock`,
`onEvokeOrb`, `triggerAtStartOfTurn`, `escapeNext`, and the integer `onPlayerGainedBlock` overload)
have defaults but no invented events. Copy/upgrade/query methods are callable by their consumers.
The pre-existing terminal `damaged_this_combat` diagnostic remains unchanged in this structural
migration; Java's `onVictory` reset and original-game oracle acceptance remain separate work.

## Checking a port

```sh
cmake --build /private/tmp/spire-build --parallel 1
ctest --test-dir /private/tmp/spire-build --output-on-failure
PYTHONPATH=/private/tmp/spire-build "$SPIRE_PYTHON" tests/replay.py /path/to/unit/traces
```

`SPIRE_PYTHON` and build configuration are described in the root README. The build checks source
and generated headers against the same device restrictions. Native and host tests establish
only their covered behavior. A unit is accepted when its original-game traces replay with zero
differences, including legal actions, card/monster state, queue state and RNG state/counters.

Never edit a recorded trace, filter comparison fields, relax types or add numeric tolerances to
make a port pass. Report an apparent oracle error with the first differing JSON path and the
Java evidence. Current oracle contract limitations are listed in the README.
