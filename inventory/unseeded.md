# Unseeded randomness

Target: `desktop-1.0.jar` SHA-256
`dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a`.
Paths below are relative to `com/megacrit/cardcrawl/` in the read-only decompilation.
`oracle/java/Patches.java` instruments these calls in the JVM; the original JAR is untouched.

## Rule stream

`rng.unseeded` is a libGDX `RandomXS128` initialized with the run seed. Its counter increments
on each `nextLong`, including rejection samples from bounded integer draws. Its snapshot is
`[counter, state0, state1]`. It is distinct from the policy RNG and the presentation RNG.
The replacement methods retain MathUtils' inclusive integer bounds, exclusive floating upper
bounds, boolean implementation and long-via-double implementation.

| Java location | Original source of randomness | Pinned behavior |
| --- | --- | --- |
| `random/Random.java:19`, constructor `Random()` | `MathUtils.random(9999L)` and `MathUtils.random(99)` | Same two calls on the rule stream; preserve the subsequent counter-advance loop in the seeded constructor. |
| `random/Random.java:31`, `copy()` | Default constructor before overwriting the copied state | Preserve both otherwise wasted rule-stream calls and the copy's final state/counter. |
| `dungeons/AbstractDungeon.java:836`, `transformCard(AbstractCard, boolean)` | Default `Random()` | Uses the patched default constructor. |
| `dungeons/AbstractDungeon.java:1027`, `returnTrulyRandomCardFromAvailable(AbstractCard)` | Default `Random()` | Uses the patched default constructor. |
| `helpers/CardLibrary.java:1026`, `getCurse(AbstractCard)` | Default `Random()` | Uses the patched default constructor. |
| `cards/CardGroup.java:483`, `getRandomCard(boolean)` | `MathUtils.random` when `useRng` is false | Rule-stream bounded integer draw; the true branch retains `cardRng`. |
| `cards/CardGroup.java:490`, `getRandomCard(boolean, CardRarity)` | `MathUtils.random` when `useRng` is false | Same filtering and sorting, then rule-stream index. |
| `cards/CardGroup.java:524`, `getRandomCard(CardType, boolean)` | `MathUtils.random` when `useRng` is false | Same filtering and sorting, then rule-stream index. |
| `monsters/exordium/Cultist.java:80`, `takeTurn()` | `MathUtils.random(1, 10)` chooses a speech branch and persists `saidPower`; `die()` changes its death timer from this flag | Rule-stream bounded integer draw. Sound choices remain on the presentation stream. |
| `monsters/MonsterGroup.java:191`, `getRandomMonster(AbstractMonster, boolean)` | Four `MathUtils.random` sites, lines 205, 207, 221, 228 | Preserve all original filtering/early returns, then rule-stream index. |
| `characters/AbstractPlayer.java:1980`, `loseRandomRelics(int)` | `MathUtils.random`, line 1989 | Rule-stream index; preserve ordered removals and `onUnequip`. |
| `actions/common/DiscardAtEndOfTurnAction.java:42`, `update()` | `Collections.shuffle(cards)` | `Collections.shuffle(cards, new java.util.Random(rule.nextLong()))`; preserve hook order after shuffling. |
| `dungeons/AbstractDungeon.java:1096`, `returnColorlessCard()` | `Collections.shuffle(colorlessCardPool.group)` | Seed a Java Random from one rule-stream long; preserve the mutation of pool order. |
| `relics/Test5.java:36`, `onEquip()` | `Collections.shuffle(upgradableCards)` | Same seeded Java shuffle. This relic is unreachable in the standard Silent pool. |
| `helpers/MonsterHelper.java:460,463,481`, `getEncounter(String)` | `MathUtils.random` for Byrd offsets | Rule-stream float calls in original left-to-right construction order. |
| `helpers/MonsterHelper.java:600,604`, `randomYOffset` / `randomXOffset` | `MathUtils.random` for monster offsets | Rule-stream float calls. X positions can determine insertion order in `SpawnMonsterAction.update`. |
| `blights/Accursed.java:46`, `onEquip()` | Default `Random()` | Covered by the patched default constructor; endless blights are outside the standard-run target. |

The `Patches.Randomness` class instruments all `MathUtils.random*` calls in the listed owning
classes (including overloads), and every one-argument `Collections.shuffle` in those classes.
The already seeded Java shuffles are untouched. A seeded `java.util.Random` used for a shuffle
is temporary; its seed is accounted for in its originating stream. All 13 dungeon RNG objects
and `NeowEvent.rng` retain their original counters and both XS128 state words. `RoomTypeAssigner`
uses `mapRng.random` directly, so its internal state is necessary even when its wrapper counter
does not advance.

## Identity and presentation

| Java location | Pin |
| --- | --- |
| `cards/AbstractCard.java:319`, constructor | Replace `UUID.randomUUID()` with `new UUID(0, ++allocator)`. Keep `makeSameInstanceOf` UUID sharing. Export UUID equality as encounter-order integers, independent of allocator offset between runs. |
| `helpers/GameTips.java:27,31`, `initialize()` / `getTip()` | Recreate the hint deck after seeding the presentation stream at run start. Shuffle with a Java Random seeded from one presentation-stream long; `getTip` uses that stream. Hint text and its refill cycle cannot consume rule RNG. |
| `com/badlogic/gdx/graphics/Texture.load`, hash used by `com/badlogic/gdx/utils/ObjectSet.push` | Texture receives a sequential identity hash while retaining reference equality. ObjectSet hash collisions otherwise consume presentation randomness according to JVM identity hashes. |
| `com/badlogic/gdx/utils/{ObjectSet,ObjectMap,IntMap,LongMap,IntIntMap,IntFloatMap,ObjectIntMap,ObjectFloatMap}.push` | Random cache eviction uses a separate device-only XS128 stream. Cold and warm resource caches cannot spend presentation or rule RNG. |
| `com/badlogic/gdx/math/MathUtils.random` | Separate `RandomXS128(0)` during bootstrap, `RandomXS128(seed)` at run start. Fixed 1/60 updates and retained rendering make presentation calls reproducible; rule sites above no longer draw from it. |
| `helpers/SeedHelper.getString(0)` / CommunicationMod `executeStartCommand` | Prefix the encoded seed with `0`. Zero otherwise encodes as an empty string and the command generates a seed from `System.nanoTime`; the prefix also avoids trial-seed names. |

Remaining MathUtils sites in standard gameplay code choose sound variants/pitch, dialogue,
animation offsets, particles, map jitter, or visual effect coordinates. In particular,
`NeowEvent.playSfx`, `GremlinWheelGame.update`'s wheel angle, and monster vocalizations do not
choose rewards or moves. They remain on the seeded presentation stream. The wheel's result
comes from `miscRng`; monster moves and HP retain their original named streams. `Soul.update`
and card/campfire effects retain original callbacks and are drained to the documented decision
boundary; their presentation timing is not replaced with invented game actions.

Wall-clock calls in rendering/logging/metrics are not rule RNG. `Metrics.gatherAllData` generates
a UUID for the run report; this identifier is not consulted by game rules or exported. The
recorder never invokes the unseeded character-select start route, daily generation, or save/load.
The Bomb's static id offset, selection-action statics, Centennial Puzzle and Orange Pellets
flags reset to fresh-process values between jobs, as do the Neow stream and draw-history lists.
These resets happen before a run, never during a combat.

Batch initialization also restores the fresh-menu `relicsToRemoveOnStart` list, room/image dialog
selection, turn/energy counters, `nextRoom` and `lastCombatMetricKey`. `AbstractDungeon.reset`
leaves some of these at their previous-run values; the cold-menu list is captured before the
first job rather than guessed or de-duplicated. Every snapshot still records the real values.

`screens/select/GridCardSelectScreen.java:258-263`, `update()`, generates a transform preview
every 0.1 seconds by calling `AbstractDungeon.returnTrulyRandomCardFromAvailable`, whose default
`Random()` consumes two rule draws. `callOnOpen()` does not reset `ritualAnimTimer`. Snapshots
retain this timer even when GRID is inactive, and retain `hoveredCard` during confirmation.
The grid screen is recreated at run start to restore its cold-process state; the timer is
never reset between selections or rooms within a run.
