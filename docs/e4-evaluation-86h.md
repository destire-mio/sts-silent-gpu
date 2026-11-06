# E4 86h evaluation — dev-78 rollback, pure PPO

The retained **dev-86** weights completed the ordinary **1,024-game argmax** exam at update **1951**, training time **309609.7414498508 seconds**, on **3 V100 GPUs**. Mean floor was **28.751953125**, with **11 Act 3 second-boss flags** and **0 Heart wins**. The paired difference versus dev-84 was **−0.2744140625**, SE **0.41218789295719366**; versus dev-70 it was **−0.59765625**, SE **0.4298621922003837**.

This exam used the Pandora grid fix. The training window spans both old and new engine phases, all under dev-78 rollback pure PPO. Fixed-seed differences do not establish a causal effect of the engine fix, removing CE, rollback, or GPU count.

## Evaluation protocol and checkpoint identity

- Seeds: **2000000000..2000001023**, all 1,024 unique; exact ownership `seeds[rank::3]`.
- Selection: ordinary argmax, no SL/search; observation version **14**. Combat/card preview modes were not used.
- Actual source: `5ac291b10e47ecbe386c6a67789692678c4a887c`, `c159-preparation/runtime`; isolated build `/home/zhuyouwei/spire-build-e4-c153-pandora`. Public engine merge: `aa91f4bc6d8c997b76788cc72681c8463b9e422e`.
- Snapshot: `dev-86.pt`, **49762853 bytes**, SHA-256 `ba9ff20394577059808b1562791bec6e54810f93f751d822a67cab57e7051632`.
- Independent CPU strict load: CLS **384 / 6 layers / 6 heads / 1536 feedforward**, **12,435,074 parameters**, all tensors finite.

The dev snapshot contains model weights only. The last independently observed natural training-state checkpoint was **1939 / clock308233.0204372878**, serial unread. The retained dev-86 weights and completed exam do not establish that update1951's Adam/collector state has been saved. Final optimizer counters below are explicitly implied from the verified trajectory; no large current rank checkpoint was read.

## Full-game outcomes

| Metric | Count / 1024 | Rate |
|---|---:|---:|
| Act 1 boss passed | 775 | 0.7568359375 |
| Act 2 boss passed | 183 | 0.1787109375 |
| Act 3 first boss passed | 43 | 0.0419921875 |
| Act 3 second boss passed / victory flag | 11 | 0.0107421875 |
| Act 4 entered | 0 | 0 |
| Shield/Spear passed | 0 | 0 |
| Heart victory | 0 | 0 |

Result/ledger labels are **1013 `death`, 11 `act3_win`, 0 `stalled`**; all 1,024 are included in the mean and paired comparisons. The existing collector labels terminal episodes that passed the Act 3 second boss without a Heart win as `act3_win` and removes death metadata. This label is not a full-game victory or an exhaustive count of deaths after Act 3.

Ruby / emerald / sapphire counts were **26 / 213 / 22**; **0** episodes finished with all three keys. No episode entered Act 4. Independent key/reach and boss-hierarchy checks found **0 contradictions**.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
|---|---:|---:|---:|
| dev-84 | −0.2744140625 | 0.41218789295719366 | 1024 |
| dev-70 | −0.59765625 | 0.4298621922003837 | 1024 |
| dev-16 teacher argmax | +11.2822265625 | 0.3759653748406895 | 1024 |

## Act 2 ledger audit

The consolidated `evaluation-1951-act2-ledger.json` contains **1,024 episodes** and **775 Act 2 entry snapshots**; **1590824 bytes**, SHA-256 `cdbee47d21a54b3e5080d729ab67c3f3c803498e61d47e9faea1595ae1380175`.

Three result shards and three ledger shards were checked independently for unique seed ownership and agreement with consolidated floors, keys, reach flags, normalized outcomes, stalled markers, available death metadata, entry decks/relics/potions/HP/gold and counters.

- Card skips / removals / rests / upgrades: **1819 / 2195 / 2985 / 2203**.
- Elites by Act 1 / 2 / 3 / 4: **1491 / 855 / 234 / 0**.
- Mean Act 2 entry deck size / HP / max HP / gold: **21.495483870967742 / 57.34967741935484 / 67.74193548387096 / 192.8025806451613**.
- Tied-rank Spearman against final floor: Act 1 boss value **0.4061334854831509** (**927 cases**); Act 2 entry value **0.4361903864155384** (**775 cases**).

## Training window, source phases and optimizer accounting

The strict learning-rate window is **(302444.91937079903, 309609.7414498508]**. All **64 complete updates** are `dev78_rollback_pure_PPO`: beta0, samples0, collectionfalse; no CE rows are invented.

| Retained job | GPUs | Source | Engine phase | Complete updates | Updates | Complete-row steps | AMP skips |
|---|---:|---|---|---|---:|---:|---:|
| 5067 | 3 | c289fa2 | old v14 | 1888–1898 | 11 | 264 | 0 |
| 5080 | 4 | 5ac291b | Pandora grid fix | 1899–1909 | 11 | 352 | 0 |
| 5081 | 5 | 5ac291b | Pandora grid fix | 1910–1913 | 4 | 160 | 0 |
| 5082 | 4 | 5ac291b | Pandora grid fix | 1914–1916 | 3 | 96 | 0 |
| 5083 | 5 | 5ac291b | Pandora grid fix | 1917–1919 | 3 | 120 | 0 |
| 5084 | 4 | 5ac291b | Pandora grid fix | 1920–1923 | 4 | 128 | 0 |
| 5103 | 3 | 5ac291b | Pandora grid fix | 1924–1951 | 28 | 672 | 0 |

There are **1792 complete-row applied optimizer steps**, plus **84 already-applied partial steps once**: **1 + 21 + 24 + 38 = 1876 total applied steps**, with **0 AMP skips**. These extra steps are identified by checkpoint identity and cursor/Adam transitions, not by rank log count or successor job count.

| Checkpoint / serial | World transition | Verified Adam at resume | Delta since previous anchor | Complete steps in delta | Applied partial once |
|---|---|---:|---:|---:|---:|
| 1898 / 205 | 3→4 | 111615 | 265 from84 anchor111350 | 264 | 1 |
| 1909 / 206 | 4→5 | 111988 | 373 | 352 | 21 |
| 1913 / 207 | 5→4 | 112148 | 160 | 160 | 0 |
| 1916 / 208 | 4→5 | 112268 | 120 | 96 | 24 |
| 1919 / 209 | 5→4 | 112426 | 158 | 120 | 38 |
| 1923 / 210 | 4→3 | 112554 | 128 | 128 | 0 |

Job5103 contributes **28 × 24 = 672** complete steps after1923 through1951, implying Adam **113226** at the evaluated update. This final counter is not claimed to be read or persisted. Through1923, **1204 = 1120 complete + 84 partial** is directly cross-checked by real successor resume counters. The periodic checkpoint1939 adds durability evidence without a new Adam/cursor read.

The new partials are **1 at1898/205, 21 at1909/206, 24 at1916/208, 38 at1919/209**, each counted once. Their total84 is not a reuse of the old partial22 at1829/200, which was counted in the84h report. Older7/3/30/16/49 also remain in their earlier windows. Old5050/5052 orphan rows are outside this window and are not reintroduced by choosing maximum clocks among duplicate IDs.

Nominal student decisions are **29,360,128 = Σ(N × 2048 × 64)**; logged valid-mainline decisions are also **29,360,128**. The evaluated update1951 has **393216 = 3 × 2048 × 64**: combat296194, card_reward9929, map18844, shop5900, campfire9478, other52871.

LR stayed **6.666666666666667e−5**, with **0 lower-bound touches**. KL ranged **0.006256371163602263..0.017465622093776667**, ending at **0.012618316399692317**. No training episode faults were observed in these retained64 updates or their corresponding selected job-log intervals. This observation does not guarantee future fault-free training.

## Last five expert-round fields

Updates **1947–1951** all use3 GPUs and explicitly log `expert_beta=0`, `expert_samples=0`, `expert_collection=false`. **N/A is an absent measurement while the expert subsystem is paused, not a measured zero.**

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| 1947 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1948 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1949 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1950 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1951 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurred in these five update intervals.

| Update | Override labels | Mean CE gradient norm | Mean PPO gradient norm for CE merge | Mean CE scale | Combat entropy |
|---:|---:|---:|---:|---:|---:|
| 1947 | 0 | N/A | N/A | N/A | 0.309804702338876 |
| 1948 | 0 | N/A | N/A | N/A | 0.30872573549504573 |
| 1949 | 0 | N/A | N/A | N/A | 0.30403195303438874 |
| 1950 | 0 | N/A | N/A | N/A | 0.30542923880645556 |
| 1951 | 0 | N/A | N/A | N/A | 0.3063308060167423 |

CE-merge norm and scale are N/A while CE is paused; ordinary PPO/AMP/clipping continues. Minibatch-mean norm and scale must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
|---:|---:|---|
| 1947 | 3 | `2482184192, 2367381504, 2367270912` |
| 1948 | 3 | `2482184192, 2367381504, 2367270912` |
| 1949 | 3 | `2482184192, 2367381504, 2367270912` |
| 1950 | 3 | `2482184192, 2367381504, 2367270912` |
| 1951 | 3 | `2482184192, 2367381504, 2367270912` |

This short RSS window does not prove absence of longer-term memory growth. Existing calibration probe warnings and prior terminal host OOM allocations are separate from expert recovery events.

## Engine transition and audit artifacts

The Pandora/CallingBell fix reset `grid_source=Grid` in the two onEquip paths. Natural job5080 used the new source/build, passed the exact five CUDA gates inside its existing allocation, discarded old unfinished live collectors/PPO cursors/four pools once while retaining seed RNG and model/Adam/scaler/c/calibration/clock/global RNG, then completed finite pure-PPO updates. Natural checkpoint1909 and job5081's resume confirmed the one-time `claude153_pandora_grid` marker. This startup/training/persistence acceptance is closed and was not repeated for this exam. It is not a long-term guarantee or a causal throughput benchmark.

Independent CPU audit of existing retained files: **exit0 / 3.9927444458007812 seconds**. Full audit SHA-256 `29aa7f6b59e7cb8ac4f1306fd37f076b7ee17d7b691e22b3ce63c93f77c82429`; script SHA-256 `d61f51802c16dd67c6133371107f67700cffd96a580042be09ba07d2fc3636cf`. The audit checked the existing model, six shards, consolidated ledger, natural report, retained own-job trajectory, LR/KL, partial identities and final five expert intervals. No GPU tests, evaluation reruns, worker interruption or forced save were performed.

This ordinary86h boundary has **no128-seed supplement**. Next ordinary evaluation: **88h = 316800 seconds**, LR window strictly greater than **309609.7414498508**. The88h128-seed argmax/combat/card supplement uses separate shared3600-second mode caps, eight logical shards distributed across actual physical workers, serial-shard sums per worker then parallel maximum per natural window and window summation. Completed seeds must not be rerun; capped modes are final, with paired completion intersections and explicit time-censored exclusions. These comparisons remain evaluation-only. The86h partials **1 / 21 / 24 / 38 are now accounted and must not be added again in88h**.
