# E0 visible observation protocol

Version 11 targets the pinned Java build in `design.md`. The source root is the read-only
`java_source_root` in `inventory/deep-neow.json`; paths below are relative to
`com/megacrit/cardcrawl/`. This protocol is separate from the oracle snapshot. Unlisted
fields are denied, including seeds, RNG words/counters, object identities, allocation slots,
queues, future reward objects, event/relic/card pools, monster move numbers and private state.
All network branches must use the same observation. Training clone/reset and evaluation
bookkeeping are control-plane operations, never policy inputs.

An observation includes the active decision screen and the free inspection views available
to a human: top panel, owned deck, relic/potion tooltips and, during combat, pile views.
Map contents are included on MAP only. Recurrent memory belongs to the caller and is built
from these observations and selected candidates, never a snapshot. Reset clears memory;
training forks copy it. Engine-internal histories are not exported.

## Numeric contract

Entity arrays are structure-of-arrays: `tags[4, E]` uint8, `ids[4, E]` int16 and
`values[6, E]` int16. Tag rows are kind, zone, packed flags and an auxiliary byte.
ID rows are catalog ID, owner pointer, visible position and variant. Zero is padding/no
reference; catalog IDs and pointers are shifted by one. Catalog indices come from the
committed `core/*/catalog.def` files. Pointers address observation entities, never engine
slots. The kind-specific numeric/flag mapping is listed below. Unused rows and padding
are zero. No raw struct bytes are exported.

Candidates are `candidate_kinds[A]` uint8 (action kind + 1) and `candidate_refs[3, A]`
int16 (subject, target, option entity). Their order remains the engine's legal-action order
from `design.md`; the private action mapping stays in the environment. All IDs, positions,
references and amount rows are checked before conversion to int16 [-32768, 32767]; byte
rows are checked against [0, 255], individual flags against [0, 1]. Card variable masks
must fit three bits. An out-of-range value invalidates the entire observation with
NumericRange, rather than saturating or wrapping. These are representation limits, not
new game rules. Shared int16 rows include values above 255 and signed costs/power amounts;
kind, zone, flags and auxiliary fields use uint8.

`entity_mask[E/8]` and `candidate_mask[A/8]` pack eight positions per byte, least significant
bit first: bit `i % 8` of byte `i / 8` is one exactly when slot i is valid. Zero remains
padding. Every count and mask is derived from the same emitted prefix. Consumers can
unpack with `np.unpackbits(mask, axis=-1, bitorder="little")`.

E0 reserves 384 entity tokens and 128 candidates. The measured maxima over the 841 committed
and 133 local deep traces are 285 and 51; `inventory/observation.json` identifies the corpus
and per-kind peaks. These are execution budgets with headroom, not Java game limits.
Seven network arrays occupy 10,176 bytes per decision (24 bytes/entity, 7 bytes/candidate,
64 mask bytes). Two int32 counts bring the padded observation to 10,184 bytes, versus
36,352 array bytes in version 1. The displayed-label sidecar and episode bookkeeping are
outside these arrays and outside the unchanged 37,872-byte `Game`.

Device callers own one `obs::History` and a zero-initialized `obs::Observation` beside each
game. Clear history on reset, call `obs::record_action(history, game, action)` before each
step, then call `obs::encode(game, history, view, private_actions)` at each settled boundary.
A borrowed `View` requires zero-initialized backing arrays/counts on first use; preserve its
counts across subsequent calls, including game resets and forks. The encoder overwrites
valid rows, clears only the previously used tail, and regenerates the used mask bytes.
Faults clear the previous and partially written prefixes. It never clears all capacities
per decision. Pile sorting initializes only populated scratch tokens, retains the version 1
public-tuple ordering and never introduces a hidden tie-breaker. Legal actions are enumerated
once, shared by candidate encoding and the next step. Encoding reads `Game` without mutation;
it records displayed labels in history. The sidecar is caller-owned memory: it may retain
facts the player saw earlier in the same event, never facts that were not displayed.
Copy history with a training fork and root cache; replay actions rebuilds it.

## Common entities

| Entity | Allowed values | Java display basis |
| --- | --- | --- |
| Screen | active screen ID; act, floor, ascension, ruby/emerald/sapphire keys | `ui/panels/TopPanel.render`, `helpers/Key` rendering; active screen's `render` |
| Player | HP, max HP, gold; block/energy only in combat | `core/AbstractCreature.renderHealth`, `characters/AbstractPlayer.render`, `ui/panels/EnergyPanel.render`, `TopPanel.render` |
| Master cards | card ID, upgrades, rendered cost and card description numbers; deck order | `screens/MasterDeckViewScreen.open/render`, `cards/AbstractCard.renderEnergy/renderDescription/getCost` |
| Relics | acquisition order, counter only if >= 0 (presence bit), grayscale and used-up tooltip | `relics/AbstractRelic.renderInTopPanel/renderCounter/usedUp`; the six unit-specific grayscale mappings follow their Java callbacks; no private counters |
| Potions | slot, ID, displayed potency | `ui/panels/TopPanel.renderPotions`, each potion's `initializeData/getPotency`; empty slots retained |
| Cards | upgrades, displayed cost, damage/block/magic only for description variables; bottle badges on master deck and noncombat grid only | `cards/AbstractCard.render/renderEnergy/renderDescription/renderDynamicVariable`; UUID, misc and internal flags denied |

Card dynamic variables use a source-audited per-card variable mask. Conditional extended
descriptions are denied: Blizzard, Stack, ThunderStrike, MindBlast and SpiritShield can
retain base-number caches after their extra text disappears. `AbstractCard.makeStatEquivalentCopy`
rebuilds the normal/upgraded description without that text. Expunger is the explicit exception:
its `makeStatEquivalentCopy` preserves the description generated by `setX`, including its hit count.
Combat hand, draw, discard, selected cards and combat grids restore version 2's live
numbers. `AbstractCard.renderDynamicVariable:1775–1822` selects the cache when its modified
flag is set. Both scalar and multi-damage calculations compare their final values with the
base (`applyPowers:2232–2235,2266–2273`, `calculateCardDamage:2327–2330,2373–2380`), including
changes from `atDamageFinalGive`. Those paths do not by themselves leave a differing cache
behind an unset flag. Base changes after the last calculation can still disagree: the engine
does not retain Java's modified flags. Exact handling of those states is an engine follow-up.
Discard damage uses the base value, preserving the Glass Knife/Soul reset rule from version 2.
Exhaust inspection uses base numbers from stat-equivalent copies (`ExhaustPileViewScreen.open`);
noncombat cards and master-deck views also use base numbers. X and unplayable costs retain their UI
meaning. Bottle identity is denied on combat copies, offers and event faces: those renderers
call `AbstractCard.render` without the bottle overlay. Only the master deck and noncombat
grid call the bottle-overlay renderers. A bottle on an owned master card does not identify
which otherwise identical combat copy the player is inspecting. Free attack/one-use free effects are reflected in the rendered cost
(`AbstractCard.getCost/freeToPlay`).

## COMBAT

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Hand | ordered visible cards; candidate subject points to the chosen card | `AbstractPlayer.renderHand`, `AbstractCard.render` |
| Draw pile | multiset sorted by the complete emitted public card tuple; position zero, duplicates retained; with Frozen Eye preserve displayed order | `screens/DrawPileViewScreen.open:218-229`; no hidden tie-breaking key |
| Discard / exhaust | discard display order; exhaust multiset unless Frozen Eye | `DiscardPileViewScreen.open/hideCards`, `ExhaustPileViewScreen.open:209-221` |
| Monsters | visible living/dying/half-dead creatures, compact public order, HP/block, max HP only above zero HP, screen x and facing | `monsters/AbstractMonster.render`, `AbstractCreature.renderHealth` |
| Intents | displayed icon; damage and multiplicity only for ATTACK variants; all three absent under Runic Dome, dying/escaping creatures, or noncombat room | `AbstractMonster.render:735`, `renderDamageRange`, `renderTip:244`; pending move, base damage and move history denied |
| Powers | ID, owner, order, amount only when >0 or negative with canGoNegative; tooltip additions below | `powers/AbstractPower.renderAmount:184-195`; unlisted private counters and object IDs denied |
| Orbs / stance | slot, type, passive/evoke numbers, visible stance | `orbs/*Orb.renderText/updateDescription`, `stances/*Stance.updateDescription`, `AbstractPlayer.render` |

`AbstractCreature.renderHealthText` displays the dead label at zero HP, so maximum HP is
denied there, including half-dead Darklings. A caller can remember earlier health displays.

`TheBombPower.updateDescription` and `PanachePower.updateDescription` display their damage;
`CombustPower.updateDescription` displays HP loss; `DevaPower.updateDescription` displays
energyGainAmount; `MalleablePower.updateDescription` displays basePower. These use numeric row 1.
`StasisPower.updateDescription` and `NightmarePower.updateDescription` display a card name:
variant is its catalog ID + 1 and numeric row 2 its upgrade suffix. The referenced card's
other stats and engine reference are denied. Invincible maxAmt, Flight storedAmount,
Echo cardsDoubledThisTurn, poison/constriction sources and just-applied flags are denied.
Intents at interrupted selection/reward overlays are omitted: the engine does not retain
the renderer's intent-alpha visibility state at those boundaries. This is a documented
information omission, not permission to expose pending moves.

## HAND_SELECT (engine Discard)

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Combat context | COMBAT fields above | `screens/select/HandCardSelectScreen.render` |
| Selection | prompt semantic ID, count required, selected visible cards, upgrade flag; options point to remaining hand | `HandCardSelectScreen.open/render/updateSelectedCards`; pending effect payload denied |

## GRID

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Grid cards | current target-group display order, selected cards, confirmation target | `screens/select/GridCardSelectScreen.open/render` |
| Selection prompt | upgrade/purge/transform, number to select, confirmation; scry's shown top cards retain shown order | `GridCardSelectScreen.open`, `actions/utility/ScryAction.update` |
| Other piles | use COMBAT rules when combat is visible; never emit grid source slot IDs | `GridCardSelectScreen.render` |

## CARD_REWARD

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Offered cards | current screen's cards, display order | `screens/CardRewardScreen.open/render` |
| Controls | skip and Singing Bowl, with candidate pointers | `CardRewardScreen.render`, `ui/buttons/SingingBowlButton` |
| Other reward lists | denied until displayed | `CardRewardScreen.render` |

## COMBAT_REWARD

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Reward rows | type, gold amount, potion/relic ID, linked sapphire-key/relic row | `rewards/RewardItem.render`, `screens/CombatRewardScreen.render` |
| Card row | generic CARD identity; generated card contents denied until CARD_REWARD | `RewardItem.render` versus `CardRewardScreen.open` |
| Controls | proceed / skip as legal candidates | `CombatRewardScreen.render` |

## EVENT (including Neow)

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Dialog | event ID, displayed page, button slot, disabled state; static effect text encoded by event/page/slot | each `events/*/*.java` constructor and `buttonEffect`; `events/GenericEventDialog`, `ui/buttons/LargeDialogOptionButton.render` |
| Dynamic text | only the page-gated values in the event whitelist below | each named class's dialog construction/update |
| Neow | reward type, drawback, displayed HP amount on blessing page only | `neow/NeowEvent.blessing`, `neow/NeowReward` constructor; generated outcome denied |
| Match and Keep | face-down card backs have no card ID/stat; shown card faces and attempt count; public spatial positions | `events/shrines/GremlinMatchGame.update/render`, `AbstractCard.render` |
| Wheel | spin control; result only after spin completes and the dialog reveals it | `events/shrines/GremlinWheelGame.preApplyResult`; preselected result denied |

`DeadAdventurer.randomReward` replaces the introduction body after each successful search.
The initial displayed enemy clue is recorded in caller-owned `History` and retained while
the event returns to INTRO, as in version 2. After the initial display, encoding reads the
recorded clue, not the private enemy field. Starting from a later state without that history
cannot recover an unseen clue. Leaving INTRO or resetting clears it.

Event dynamic whitelist (unlisted event state denied):

| Event and displayed page | Values | Java basis |
| --- | --- | --- |
| Neow blessing (3) | choice, drawback, hp_bonus | `NeowReward` description construction |
| Falling choice (1) | offered master-card faces, not UUIDs | `Falling.buttonEffect`, OPTIONS 1/3/5 |
| Designer main (1) | one upgrade vs two random, remove vs transform; costs 50/75/110, damage 5 | `Designer.buttonEffect`, OPTIONS 1-13 |
| WeMeetAgain intro (0) | offered potion/card, goldAmount | constructor OPTIONS 0/2/4 |
| Nloth intro (0) | offered relics | constructor OPTIONS 0 |
| DeadAdventurer intro (0) | encounterChance; initial enemy clue remembered through History; no shuffled rewards | constructor body DESCRIPTIONS 3/4/5; `randomReward` |
| KnowingSkull ask (1) | potion/gold/card/leave HP costs, 90 gold | `obtainReward` dialog updates |
| ScrapOoze intro (0) | dmg, relicObtainChance | constructor and `buttonEffect` |
| GoopPuddle intro (0) | goldLoss, 75 gold, 11 damage | constructor |
| BigFish intro (0) | healAmt | constructor |
| Cleric intro (0) | healAmt and purifyCost only on enabled matching buttons | constructor |
| ForgottenAltar intro (0) | hpLoss | constructor |
| GoldenIdolEvent escape choice (1) | damage, maxHpLoss | `buttonEffect` |
| Ghosts / Vampires intro (0) | hpLoss / maxHpLoss | constructor |
| MoaiHead intro (0) | hpAmt | constructor |
| ShiningLight intro (0) | damage on enabled button | constructor |
| TheLibrary intro (0) | healAmt | constructor |
| WindingHalls choice (1) | hpAmt, healAmt, maxHPAmt | `buttonEffect` |
| CursedTome last page (4) | finalDmg | `buttonEffect` |
| FaceTrader main (1) | damage, goldReward | `buttonEffect` |
| GremlinMatchGame play (2) | attemptCount, visible faces | `render/update` |
| GremlinWheelGame complete (1) | revealed result, displayed gold/damage | `preApplyResult` |

Match positions are compact left-to-right order of remaining cards. Currently face-up
cards are emitted. `GremlinMatchGame.updateMatchGameLogic` also displays the clicked second
face for 1.25 seconds before covering a mismatch. The caller's action recorder saves only
that clicked face as a `History` card token for the next settled boundary. This restores
version 2's lifetime: repeated encoding and copying preserve it, and the next action clears
or replaces it. Other covered faces never enter history. Earlier faces beyond that single
boundary require caller memory; the sidecar does not expose the hidden board.

The sidecar also caches RestOption's displayed healing numbers on entry to each rest-room
floor. Java constructs that description once, so later Fruit Juice use changes the top-panel
max HP but not the button's text. Reset clears the sidecar and clone copies it.

## MAP

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Nodes and edges | current act's displayed room icon, x/y, edges, emerald marker, taken and current marker | `map/MapRoomNode.render`, `screens/DungeonMapScreen.render` |
| Boss | current announced boss only | `map/DungeonMap.renderBossIcon`; second A20 boss list denied |
| Question marks | Event icon only; never event ID, encounter, reward or RNG | `MapRoomNode.render`; `rooms/EventRoom.onPlayerEntry` resolves contents after entry |

## ROOM (campfire / chest / shop entrance / exit)

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Campfire | displayed option kind, disabled flag, heal amount / Lift count | `rooms/RestRoom.render`, `ui/campfire/*Option` constructors/render |
| Chest | visible chest size/open state, open action; unopened contents denied | `rewards/chests/*Chest.render` |
| Merchant entrance | shop control only; stock denied until SHOP | `shop/Merchant.render`, `ShopScreen.open/render` |
| Exit | proceed control | `ui/buttons/ProceedButton.render` |

## SHOP

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Stock | all displayed cards/relics/potions including unaffordable ones, prices, public slots | `shop/ShopScreen.render`, `shop/StoreRelic.render`, `shop/StorePotion.render` |
| Removal | displayed purge price/availability | `ShopScreen.renderPurge` |
| Candidates | affordable choices in original order, mapped onto stock entities; exit | `ShopScreen` purchase/update methods; no future restock |

## BOSS_REWARD

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Relics | three currently offered relic IDs, public position, skip | `screens/select/BossRelicSelectScreen.open/render` |
| Next act | denied until entered | `dungeons/AbstractDungeon` transition |

## VICTORY

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Terminal | victory screen, common owned objects; no candidates | `screens/VictoryScreen`; engine reaches it through original victory transition |
| Reward | one transition reward 1; three keys / Heart route tested | `rooms/TrueVictoryRoom`, `monsters/ending/CorruptHeart.die` |

## DEATH

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Terminal | death screen, common owned objects; no candidates; reward 0 | `screens/DeathScreen`, `AbstractPlayer.damage` |

## NONE

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| No decision | no candidates; batch reports a fault if encountered as a live training state | engine's settlement boundary in `design.md`; no policy decision exists here |

Faults and decision-cap truncation are distinct from game death and victory. Cap cuts preserve
the final observation for value bootstrapping; faults invalidate it. Neither becomes a loss
sample. CPU core assertions retain the existing process-abort contract; E0 does not catch
signals or change `core/`. Latched engine faults and observation faults have batch fault codes.

## Feature rows

All rows not named here are zero. IDs/positions use the one-based convention above;
numeric amounts/coordinates are unshifted. A card cost of -1 means X; -2 means no printed cost.

| Kind | int16 numeric rows (0–5) | Flag bits (low to high) | Auxiliary byte |
| --- | --- | --- | --- |
| Screen | act, floor, ascension, event page, requested count (0 at confirmation), selected count | ruby, emerald, sapphire, any-number, can-pick-zero (HAND_SELECT), confirmation | selection prompt |
| Player | HP, max HP, gold, block, energy, 0 | 0 | 0 |
| Card | upgrades, printed cost, zone-specific damage, block, magic, shop price | description D/B/M in bits 0–2; bottle flame/lightning/tornado in 3–5 (master/noncombat grid only); grid selected/confirmation target in 6–7 | Match attempts remaining |
| Monster | HP, max HP (zero when HP is zero), block, screen x, shown damage, shown hits | facing | shown intent + 1 (0 hidden) |
| Power | shown amount, whitelisted tooltip number, referenced card upgrade suffix, 0, 0, 0 | amount-visible | 0 |
| Relic | shown counter, shop price, 0, 0, 0, 0 | counter-visible, grayscale, used-up tooltip | 0 |
| Potion | owned potency, shop price, 0, 0, 0, 0 | 0 | 0 |
| Orb | passive, evoke, 0, 0, 0, 0; empty slot has ID 0 and valid mask | 0 | 0 |
| Stance | all zero | 0 | 0 |
| MapNode | x, y, destination-column bitset, 0, 0, 0; map row 14 reaches boss, Act 4 row 2 reaches Heart | emerald, taken, current | 0 |
| Reward | displayed gold, linked reward entity, 0, 0, 0, 0; variant = offered relic/potion ID + 1 | 0 | 0 |
| Option | control-specific: purge 0 price; campfire 0 disabled, 1 rest heal, 2 Regal Pillow bonus; chest 0 open, 1 size; event 0 disabled, 1–3 whitelisted parameters, 4 page; Match back 5 attempts | 0 | 0 |

Screen variant is event ID + 1 on EVENT and room kind + 1 on ROOM. Event option variant is
event ID + 1; campfire variant is option kind + 1; boss variant is announced encounter ID + 1.
Card variables are numeric metadata extracted from the pinned English descriptions by
`host/generate_observation_cards.py`; no game text or assets are stored in this repository.
The base/upgrade masks include extended description variables used by dynamic card text.
Selection prompt IDs are an explicit semantic whitelist: 1 upgrade, 2 purge, 3 transform,
4 discard, 5 exhaust, 6 retain, 7 put on top, 8 put on bottom, 9 duplicate, 10 nightmare,
11 setup, 12 scry, 13 omniscience, 14 retrieve (including Meditate); zero means no additional prompt.
They encode the displayed selection message, not an action queue or object number.
Meditate and BetterDiscardPileToHand use the same `BetterToHandAction` UI strings and
therefore share prompt 14; the hidden continuation effect cannot distinguish them.

## Batch interface and episode boundaries

`spire.TrainingBatch(size, threads, decision_cap, seeds)` owns CPU games and persistent worker
threads. `seeds` is a finite caller-supplied sequence of signed Java seeds. Initial reset and
every later reset consume it in ascending slot order, independent of worker scheduling.
`extend_seeds(seeds)` appends to that sequence. Exhaustion is a fault, never seed reuse.
Size, thread count and cap must be positive, with threads <= size.

Cache the following NumPy views once; all storage is allocated at construction and remains
stable. Views keep the owning batch alive and are read-only to Python. Every `step(indices)`
requires a contiguous int32 `[N]` array. It releases the GIL and returns no dictionaries.

| Buffer | Shape / dtype | Meaning |
| --- | --- | --- |
| tags | `[N, 4, E]`, uint8 | kind, zone, packed flags, auxiliary |
| ids / values | `[N, 4, E]` / `[N, 6, E]`, int16 | catalog/references and numeric rows |
| candidate_kinds / candidate_refs | `[N, A]` uint8 / `[N, 3, A]` int16 | action kind and public references |
| entity_mask / candidate_mask | `[N, E/8]` / `[N, A/8]`, uint8 | packed padding masks |
| rewards | `[N]`, float32 | 1 on the step reaching Victory, 0 otherwise |
| done | `[N]`, uint8 | true game victory/death transition |
| truncated | `[N]`, uint8 | decision cap, with final observation retained |
| faults | `[N]`, uint32 | engine latched code or E0 code; nonzero invalidates observation |
| reset_mask | `[N]`, uint8 | this output begins a new episode; clear recurrent memory |
| advanced | `[N]`, uint8 | an action was applied; reset-only calls have zero here |
| entity_counts / candidate_counts | `[N]`, int32 | valid lengths, also derivable from masks |
| seed_indices / decision_counts | `[N]`, int64 | control-plane root-family accounting and episode decisions; never network input |
| timing_ns | `[N, 3]`, int64 | cumulative worker wall time for reset, core step and encode including legal enumeration; never network input |

After done/truncated/fault, the next call resets that slot and ignores its supplied action
index. It produces reward 0 and advanced 0. A terminal step at exactly the cap is terminal,
not truncated. No old episode is stepped or paid after it ends. Use truncated's final
observation for bootstrapping before the reset-only call overwrites the buffers. Faults
produce zero entity/candidate arrays and masks. Seed exhaustion also produces zero arrays.

E0 fault codes are `0xe0000001` entity overflow, `0xe0000002` candidate overflow,
`0xe0000003` no live decision, `0xe0000004` invalid candidate index and `0xe0000005`
seed exhaustion, `0xe0000006` numeric range and `0xe0000007` insufficient caller storage.
Storage exhaustion is returned by `store` and does not end or modify a game. Other nonzero
codes are the unchanged core's latched assertion IDs.
Host core invariant violations still abort the process, including in release builds.

`clone(i, j)` copies the game, displayed-label sidecar, root-family identity and episode decision count;
it refreshes j's observation, emits no reward and consumes no seed. The caller copies its
network memory and policy RNG separately. Mutating calls on a batch serialize; caller-owned
input arrays and borrowed output views must not be read/written concurrently with a call.
An inference worker should receive only the seven network arrays, never the batch object,
snapshot/search APIs, seed bookkeeping or the training-only clone method.

`reset_to(slot, seed, candidates)` starts a fresh Neow game and replays the candidate-index
prefix, recording encoding every boundary to rebuild `History`.
`reset_to_many(slots, seeds, candidates)` takes equally sized lists; each element of
`candidates` is one prefix, and slots are distinct. Both release the GIL and serialize with
other batch mutations. The root must be a live decision. An invalid candidate or terminal
root produces a fault, never a reward. Prefix replay ignores the continuation decision cap;
the returned root has decision count zero, reset_mask one, and reward/advanced/done/truncated
zero. Explicit resets do not consume the scheduled seed sequence; seed_indices is -1 because
the seed came from the caller. The trainer retains the actual seed as the root-family key.
Reset, replay stepping and encoding accumulate in the existing timing buffers.

`step_slots(slots, candidate_indices)` steps only the distinct selected slots, with the same
autoreset contract as `step`. Inactive games, observations and terminal/fault flags stay in
place; reward, advanced and reset_mask are zero for inactive slots. This lets synchronous
full-episode collection finish its fixed start group without advancing or resetting finished
lanes. The two arguments are equally sized lists; empty lists advance nothing.

`Game.observation()` is an allocation-based audit convenience returning tags, ids, values,
candidate kinds, candidate refs, entity mask, candidate mask and a fault code. Training uses the preallocated batch interface. `tools/check_observation.py`
uses the unchanged oracle comparator while encoding every decision. The seed-family contract
is in `inventory/seed-partitions.json`; `tools/evaluation.py` supplies Wilson intervals and
same-root paired counts, retaining fault/truncated pairs as separate counts.

## Compact rollout storage

`batch.store(slots, data, offsets)` copies selected current observations to caller-owned
contiguous NumPy buffers: slots int32 `[M]`, data uint8 `[capacity]`, offsets uint64 `[M+1]`.
Set offsets[0] to the append position; pass a slice of the episode's existing offset table
for subsequent appends. Repeated slots and arbitrary slot order are supported. This call
releases the GIL and serializes with step/clone. It returns zero on success. Insufficient
capacity returns StorageCapacity before any data/offset writes. Selecting a faulted slot
returns that fault before writing; its partial observation never enters rollout storage.
It does not advance games, consume RNG or change rewards. Save the decision observation
before stepping, and save terminal/truncated observations separately when needed.

Each record is four header bytes (entity count and candidate count, unsigned 16-bit little
endian) followed by valid prefixes only, in this order: four uint8 tag rows, four int16 ID
rows, six int16 numeric rows, uint8 candidate kinds, three int16 candidate-reference rows.
All words are little endian; records need no alignment or end padding. There are no masks
in storage: counts determine them. Size is `4 + 24*entities + 7*candidates` bytes, plus one
8-byte offset per decision and one final 8-byte offset for the entire stream. Reward/action/
return/recurrent-state storage belongs to the trainer and is not included in these sizes.
The format is tied to protocol version 11; it is not a save/load format for games.

Create `spire.ObservationBatch(minibatch_size)` once. `minibatch.pad(data, offsets, indices)`
gathers records by int32 indices into its preallocated dense arrays with the same shapes,
dtypes, padding, counts and masks as TrainingBatch. Cache those read-only NumPy views once;
subsequent pad calls keep their addresses. This call releases the GIL; do not access its
buffers concurrently or overlap pad calls on one minibatch. Input records and their offsets
must come from this protocol's store operation, with the complete referenced data buffer; the helper trusts that format rather than validating
engine-produced records. Only the selected minibatch is expanded.
`obs::store` and `obs::pad` implement the same record format in device C++ without allocation.
