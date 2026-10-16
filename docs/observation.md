# E0 visible observation protocol

Version 1 targets the pinned Java build in `design.md`. The source root is the read-only
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

Entity arrays are structure-of-arrays: `ids[6, E]` and `values[16, E]`, both signed int32,
with `entity_mask[E]` uint8. The six ID rows are kind, catalog ID, zone, owner pointer,
visible position, and variant. Zero is padding/no reference; catalog IDs and pointers
are shifted by one. Catalog indices come from the committed `core/*/catalog.def` files.
Pointers address observation entities, never engine slots. Numeric rows are kind-specific
as defined below. Unused rows and padding are zero. No raw struct bytes are exported.

Candidates are `candidates[4, A]` int32: action kind + 1, subject entity, target entity,
option entity; `candidate_mask[A]` uint8. Their order is the engine's legal-action order
from `design.md`; card/potion/option indices are replaced by public entity pointers. The
private action mapping stays in the environment. Capacity overflow invalidates the entire
observation and reports a fault; partial observations are never delivered as valid inputs.

E0 reserves 384 entity tokens and 128 candidates. The measured maxima over the 841 committed
and 133 local deep traces are 285 and 51; `inventory/observation.json` identifies the corpus
and per-kind peaks. These are execution budgets with headroom, not Java game limits.
The five network arrays occupy 36,352 bytes per environment decision, including padding.
The visible-history sidecar and episode bookkeeping are outside those network arrays and
outside the unchanged 37,872-byte `Game`.

Device callers own one `obs::History` beside each game. Clear it on reset, call
`obs::record_action(history, game, action)` before stepping, then call
`obs::encode(game, history, view, private_actions)` at every settled decision boundary.
Encoding reads `Game` without mutation; it records only the visible RestOption label in
the sidecar. Copy the sidecar with a training fork. The CPU batch implements this ordering.

## Common entities

| Entity | Allowed values (numeric rows from zero) | Java display basis |
| --- | --- | --- |
| Screen | active screen ID; act, floor, ascension, ruby/emerald/sapphire keys | `ui/panels/TopPanel.render`, `helpers/Key` rendering; active screen's `render` |
| Player | HP, max HP, gold; block/energy only in combat | `core/AbstractCreature.renderHealth`, `characters/AbstractPlayer.render`, `ui/panels/EnergyPanel.render`, `TopPanel.render` |
| Master cards | card ID, upgrades, rendered cost and card description numbers; deck order | `screens/MasterDeckViewScreen.open/render`, `cards/AbstractCard.renderEnergy/renderDescription/getCost` |
| Relics | acquisition order, counter only if >= 0 (presence bit), grayscale and used-up tooltip | `relics/AbstractRelic.renderInTopPanel/renderCounter/usedUp`; the six unit-specific grayscale mappings follow their Java callbacks; no private counters |
| Potions | slot, ID, displayed potency | `ui/panels/TopPanel.renderPotions`, each potion's `initializeData/getPotency`; empty slots retained |
| Cards | upgrades, displayed cost, damage/block/magic only for description variables; bottle badges | `cards/AbstractCard.render/renderEnergy/renderDescription/renderDynamicVariable`; UUID, misc and internal flags denied |

Card dynamic variables use a source-audited per-card variable mask. Noncombat/deck displays
use base numbers; combat hand/draw/discard use displayed caches. Exhaust inspection uses
stat-equivalent copies (`ExhaustPileViewScreen.open`), not the exhausted card's stale cache.
X and unplayable costs retain their UI meaning. Free attack/one-use free effects are reflected
in the rendered cost (`AbstractCard.getCost/freeToPlay`).

## COMBAT

| Allowed | Representation and exclusion | Java basis |
| --- | --- | --- |
| Hand | ordered visible cards; candidate subject points to the chosen card | `AbstractPlayer.renderHand`, `AbstractCard.render` |
| Draw pile | multiset sorted by the complete emitted public card tuple; position zero, duplicates retained; with Frozen Eye preserve displayed order | `screens/DrawPileViewScreen.open:218-229`; no hidden tie-breaking key |
| Discard / exhaust | discard display order; exhaust multiset unless Frozen Eye | `DiscardPileViewScreen.open/hideCards`, `ExhaustPileViewScreen.open:209-221` |
| Monsters | visible living/dying/half-dead creatures, compact public order, HP/max/block, screen x and facing | `monsters/AbstractMonster.render`, `AbstractCreature.renderHealth` |
| Intents | displayed icon; damage and multiplicity only for ATTACK variants; all three absent under Runic Dome, dying/escaping creatures, or noncombat room | `AbstractMonster.render:735`, `renderDamageRange`, `renderTip:244`; pending move, base damage and move history denied |
| Powers | ID, owner, order, amount only when >0 or negative with canGoNegative; tooltip additions below | `powers/AbstractPower.renderAmount:184-195`; unlisted private counters and object IDs denied |
| Orbs / stance | slot, type, passive/evoke numbers, visible stance | `orbs/*Orb.renderText/updateDescription`, `stances/*Stance.updateDescription`, `AbstractPlayer.render` |

`TheBombPower.updateDescription` and `PanachePower.updateDescription` display their damage;
`CombustPower.updateDescription` displays HP loss; `DevaPower.updateDescription` displays
energyGainAmount; `MalleablePower.updateDescription` displays basePower. These use numeric row 2.
`StasisPower.updateDescription` and `NightmarePower.updateDescription` display a card name:
variant is its catalog ID + 1 and numeric row 3 its upgrade suffix. The referenced card's
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

Event dynamic whitelist (unlisted event state denied):

| Event and displayed page | Values | Java basis |
| --- | --- | --- |
| Neow blessing (3) | choice, drawback, hp_bonus | `NeowReward` description construction |
| Falling choice (1) | offered master-card faces, not UUIDs | `Falling.buttonEffect`, OPTIONS 1/3/5 |
| Designer main (1) | one upgrade vs two random, remove vs transform; costs 50/75/110, damage 5 | `Designer.buttonEffect`, OPTIONS 1-13 |
| WeMeetAgain intro (0) | offered potion/card, goldAmount | constructor OPTIONS 0/2/4 |
| Nloth intro (0) | offered relics | constructor OPTIONS 0 |
| DeadAdventurer intro (0) | encounterChance and enemy clue; no shuffled rewards | constructor body DESCRIPTIONS 3/4/5; `randomReward` |
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

Match positions are compact left-to-right order of remaining cards. A `History` card token
after a click records the face the player just revealed, with its position in the preceding
observation. This preserves the second face of a mismatched pair that Java shows for 1.25
seconds before turning both cards down (`GremlinMatchGame.update`). The sidecar is written
only for the clicked card, cleared on the next action/reset, and copied on a training fork.
It is outside `Game`. No face-down unclicked card enters history.
The same sidecar caches RestOption's displayed healing numbers on entry to each rest-room
floor. Java constructs that description once, so later Fruit Juice use changes the top-panel
max HP but not the button's text. Reset clears this cache and clone copies it.

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

| Kind | Numeric rows |
| --- | --- |
| Screen | 0 act, 1 floor, 2 ascension, 3-5 keys; EVENT: 6 page; selection: 7 prompt, 8 requested count (zero at confirmation), 9 selected count, 10 any-number, 11 can-pick-zero (HAND_SELECT only), 12 confirmation |
| Player | 0 HP, 1 max HP, 2 gold, 3 block, 4 energy |
| Card | 0 upgrades, 1 printed cost, 2 description-variable mask (D=1/B=2/M=4), 3 damage, 4 block, 5 magic, 6-8 bottle badges; SHOP: 11 price; GRID: 12 selected, 13 confirmation target |
| Monster | 0 HP, 1 max HP, 2 block, 3 screen x, 4 facing; 6 shown intent + 1 (zero hidden), 7 shown damage, 8 shown hits |
| Power | 0 amount-visible, 1 amount, 2 whitelisted tooltip number, 3 referenced card upgrade suffix |
| Relic | 0 counter-visible, 1 counter, 2 grayscale, 3 used-up tooltip; SHOP: 11 price |
| Potion | owned slot: 0 potency; SHOP: 11 price; offered potion ID identifies its description |
| Orb | 0 passive amount, 1 evoke amount; empty slot has catalog ID zero and a valid entity mask |
| MapNode | 0 x, 1 y, 2 destination-column bitset, 3 emerald, 4 taken, 5 current; row 14 edges reach the boss, Act 4 row 2 reaches the Heart |
| Reward | 0 displayed gold, 1 linked reward entity; variant = offered relic/potion ID + 1 |
| Option | control-specific: purge 0 price; campfire 0 disabled, 1 rest heal, 2 Regal Pillow bonus; chest 0 open, 1 size; event 0 disabled, 1-3 whitelisted parameters, 9 page; match card/back 10 attempts remaining |

Screen variant is event ID + 1 on EVENT and room kind + 1 on ROOM. Event option variant is
event ID + 1; campfire variant is option kind + 1; boss variant is announced encounter ID + 1.
Card variables are numeric metadata extracted from the pinned English descriptions by
`host/generate_observation_cards.py`; no game text or assets are stored in this repository.
The base/upgrade masks include extended description variables used by dynamic card text.
Selection prompt IDs are an explicit semantic whitelist: 1 upgrade, 2 purge, 3 transform,
4 discard, 5 exhaust, 6 retain, 7 put on top, 8 put on bottom, 9 duplicate, 10 nightmare,
11 setup, 12 scry, 13 omniscience, 14 retrieve, 15 meditate; zero means no additional prompt.
They encode the action's displayed selection message, not an action queue or object number.

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
| ids / values | `[N, 6, E]` / `[N, 16, E]`, int32 | network entity input |
| candidates | `[N, 4, A]`, int32 | network candidate input |
| entity_mask / candidate_mask | `[N, E]` / `[N, A]`, uint8 | network padding masks |
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
seed exhaustion. Other nonzero codes are the unchanged core's latched assertion IDs.
Host core invariant violations still abort the process, including in release builds.

`clone(i, j)` copies the game, visible-history sidecar, root-family identity and episode decision count;
it refreshes j's observation, emits no reward and consumes no seed. The caller copies its
network memory and policy RNG separately. Mutating calls on a batch serialize; caller-owned
input arrays and borrowed output views must not be read/written concurrently with a call.
An inference worker should receive only the five network arrays, never the batch object,
snapshot/search APIs, seed bookkeeping or the training-only clone method.

`Game.observation()` is an allocation-based audit convenience returning the five arrays and
a fault code. Training uses the preallocated batch interface. `tools/check_observation.py`
uses the unchanged oracle comparator while encoding every decision. The seed-family contract
is in `inventory/seed-partitions.json`; `tools/evaluation.py` supplies Wilson intervals and
same-root paired counts, retaining fault/truncated pairs as separate counts.
