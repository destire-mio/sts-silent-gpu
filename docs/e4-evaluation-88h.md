# E4 88h evaluation — pure PPO, 学习率封顶 4.44e-5

> **#156 correction — card 作废：推演未随机化，偷看真实未来。** The card rows and raw counts below are retained for audit only. Their +3.3046875 headline delta, common-subset comparisons and card-versus-combat difference are **not valid evidence of policy improvement**. Ordinary argmax and combat results are not invalidated by this correction. #157 supersedes the temporary card disablement: from96h the comparison is to use argmax/combat/card with whole-run randomization for card. This does not validate the historical88h card results.

The retained **dev-88** weights completed the ordinary **1,024-game argmax** exam at update **2012**, training time **316912.1883957208 seconds**, on **6 V100 GPUs**. Mean floor was **29.8779296875**, with **13 Act 3 second-boss flags** and **0 Heart wins**. The paired difference versus dev-86 was **+1.1259765625**, SE **0.4090527484265429**; versus dev-70 it was **+0.5283203125**, SE **0.4190249489348029**.

The exam used the Pandora grid fix and the learning-rate cap. The training window contains a pre-cap segment and a capped segment, separated below. Fixed-seed differences do not establish that the cap, engine fix, rollback, removing CE, or GPU count caused the change.

## Evaluation protocol and model identity

- Seeds: **2000000000..2000001023**, all 1,024 unique; exact ownership `seeds[rank::6]`.
- Ordinary argmax, no SL/search, observation version **14**. The separate 128-seed supplement is described below.
- Actual source: `48903cb3b689cddcda14f1b3b46874a1af0da49c`, `c162-preparation/runtime`; build `/home/zhuyouwei/spire-build-e4-c153-pandora`.
- Learning-rate metadata: `learning_rate_cap=4.444444444444445e-5`, **学习率封顶 4.44e-5**. Public cap commit: `45f358ddcfb23b0bae4e6ac5a51a312fd2e3ce6f`.
- Snapshot: `dev-88.pt`, **49762853 bytes**, SHA-256 `d12f4ad874a2bc0de7559aa76a3c1ed7525067622f19f8a94124f2c833abd13f`.
- Independent CPU strict load: CLS **384 / 6 layers / 6 heads / 1536 feedforward**, **12,435,074 parameters**, all tensors finite.

The dev snapshot contains model weights only. The last independently confirmed natural training-state checkpoint was **2007 / clock316284.9746130208 / serial220**, with Adam **114919** read from the next canonical resume. The dev-88 exam does not establish persistence of update2012's Adam/collector state. No large current rank checkpoint was read.

## Full-game outcomes

| Metric | Count / 1024 | Rate |
| --- | --- | --- |
| Act 1 boss passed | 831 | 0.8115234375 |
| Act 2 boss passed | 215 | 0.2099609375 |
| Act 3 first boss passed | 47 | 0.0458984375 |
| Act 3 second boss passed / victory flag | 13 | 0.0126953125 |
| Act 4 entered | 2 | 0.001953125 |
| Shield/Spear passed | 2 | 0.001953125 |
| Heart victory | 0 | 0 |

Result/ledger labels are **1010 `death`, 13 `act3_win`, 1 `stalled`**; all 1,024 are included in the mean and paired comparisons. The existing collector labels terminal episodes that passed the Act 3 second boss without a Heart win as `act3_win` and removes death metadata. This label is not a full-game victory or an exhaustive count of deaths after Act 3.

Ruby / emerald / sapphire counts were **41 / 208 / 73**; **4** episodes finished with all three keys. Seeds **2000000183** and **2000000581** entered Act 4 and passed Shield/Spear, both ending at floor56 with all three keys and Heartfalse. Independent key/reach and boss-hierarchy checks found **0 contradictions**.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
| --- | --- | --- | --- |
| dev-86 | +1.1259765625 | 0.4090527484265429 | 1024 |
| dev-70 | +0.5283203125 | 0.4190249489348029 | 1024 |
| dev-16 teacher argmax | +12.408203125 | 0.3886572150370202 | 1024 |

## Act 2 ledger audit

The consolidated `evaluation-2012-act2-ledger.json` contains **1,024 episodes** and **831 Act 2 entry snapshots**; **1652108 bytes**, SHA-256 `00dffe937131ef4755d4dd2c7a8133d9ac28187e8ae230596183e9d56546951e`.

Six result shards and six ledger shards were independently checked for unique seed ownership and agreement with consolidated floors, keys, reach flags, normalized outcomes, stalled markers, available death metadata, entry decks/relics/potions/HP/gold and counters.

- Card skips / removals / rests / upgrades: **2222 / 2353 / 2850 / 2529**.
- Elites by Act 1 / 2 / 3 / 4: **1322 / 865 / 260 / 2**.
- Mean Act 2 entry deck size / HP / max HP / gold: **21.34777376654633 / 57.93742478941035 / 67.72202166064982 / 186.77496991576413**.
- Tied-rank Spearman against final floor: Act 1 boss value **0.4007031563818979** (**946 cases**); Act 2 entry value **0.5183804588326084** (**831 cases**).

## Training window, source and learning-rate phases

The strict learning-rate window is **(309609.7414498508, 316912.1883957208]**. All **61 complete updates** are `dev78_rollback_pure_PPO`: beta0, samples0, collectionfalse. Every segment uses the Pandora grid engine fix.

| Retained job | GPUs | Source | LR phase | Complete updates | Rows | Complete-row steps | AMP skips |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 5103 | 3 | 5ac291b | pre-cap 6.666666666666667e-5 | 1952–1991 | 40 | 960 | 0 |
| 5112 | 4 | 48903cb | capped 4.444444444444445e-5 | 1992–1995 | 4 | 128 | 0 |
| 5113 | 5 | 48903cb | capped 4.444444444444445e-5 | 1996–1998 | 3 | 120 | 0 |
| 5114 | 6 | 48903cb | capped 4.444444444444445e-5 | 1999–2001 | 3 | 144 | 0 |
| 5115 | 5 | 48903cb | capped 4.444444444444445e-5 | 2002–2004 | 3 | 120 | 0 |
| 5116 | 7 | 48903cb | capped 4.444444444444445e-5 | 2005–2007 | 3 | 168 | 0 |
| 5117 | 6 | 48903cb | capped 4.444444444444445e-5 | 2008–2012 | 5 | 240 | 0 |

Updates **1952–1991** remained at **6.666666666666667e-5**. The new source began at **1992**, with current and next LR **4.444444444444445e-5** through2012. The old segment is not described as capped. KL>.02 division by1.5 and the lower bound1e-5 remain unchanged. There were **0 lower-bound touches**. KL ranged **0.0058187438349556935.ly. Old86 partials1/21/24/38,84 partial22,82 partial7 and older3/30/16/49 remain in their earlier windows. They must not be added here or again at90h. Retained lineage, not maximum clocks among duplicate IDs or repeated rank/job logs, selects the rows. Old5050/5052 orphans stay excluded.

Nominal student decisions are **30,801,920 = Σ(N × 2048 × 64)**; valid-mainline decisions are **30,801,919**. The one-action difference is from the new fault below. Decision-type counts count attempted actions, including that invalid fault action; valid-mainline excludes it. Update2012 has **786432 =6 ×2048 ×64**: combat599772, card_reward19756, map36550, shop10621, campfire18712, other101021.

## Newly retained training episode fault

Job **5114**, after update **1998** and before1999, recorded seed **1180012898**, reason **`engine_fault:1078467217`**, parent slot **1837**, probe slot **null**, start kind **`act2_boss`**, with **1468 candidate actions**. This occurred on actual source489/c162 with the Pandora build. At the ordinary audit its cause was not established. Subsequently, main #155 reported a Mac replay at action1467 with the original two randomizations, locating `CardArena::add`'s `index < card_capacity` assertion at `core/state.hpp:71` (capacity221) during a long floor50 Boss fight. The future report label is **牌库容量上限** (card arena capacity limit). This is the reported Mac diagnosis, not a new server replay; per #155 the record is retained, training continues and the engine is unchanged. It does not identify a recurrence of the older `contains(index)` defect.

The exact **4669-byte** original line, including its final LF and original decimal randomizations, is published as [the raw fault record](faults/episode-fault5114-seed1180012898-after1998.json), SHA-256 `3123ad6392322f6a46f885661fb615b75e6253884d10c59464384a5e6ba71262`. Randomizations are **(265, 1638528552638067993)** and **(573, 7787870625558990862)**; they were preserved as original decimal bytes without a JavaScript Number round trip.

Update1999 remained finite with48 applied steps,0 AMP skips,786431 valid actions versus786432 nominal, and episode_faults1. Later rows, including2000–2012, report fault0. The fault is not a win or a terminal job failure; the invalid action is excluded from valid mainline. Earlier valid actions may still have affected learning. No replay, speculative engine patch, worker interruption or forced save was performed. This single-window observation is not a long-term fault-free guarantee.

## Last five expert-round fields

Updates **2008–2012** all use6 GPUs and explicitly log `expert_beta=0`, `expert_samples=0`, `expert_collection=false`. **N/A means an absent measurement while the expert subsystem is paused, not a measured zero.**

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2008 | 6 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2009 | 6 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2010 | 6 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2011 | 6 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2012 | 6 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurred in these five update intervals.

| Update | Override labels | Mean CE gradient norm | Mean PPO gradient norm for CE merge | Mean CE scale | Combat entropy |
| --- | --- | --- | --- | --- | --- |
| 2008 | 0 | N/A | N/A | N/A | 0.2939608511607281 |
| 2009 | 0 | N/A | N/A | N/A | 0.29454763370106873 |
| 2010 | 0 | N/A | N/A | N/A | 0.2975707876729573 |
| 2011 | 0 | N/A | N/A | N/A | 0.29977935477128437 |
| 2012 | 0 | N/A | N/A | N/A | 0.3032654545939799 |

CE-merge norm and scale are N/A while CE is paused; ordinary PPO/AMP/clipping continues. Minibatch-mean norm and scale must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
| --- | --- | --- |
| 2008 | 6 | `2313764864, 2274533376, 2278273024, 2269274112, 2264055808, 2264154112` |
| 2009 | 6 | `2345877504, 2304811008, 2309468160, 2297716736, 2298003456, 2295349248` |
| 2010 | 6 | `2348630016, 2304811008, 2309468160, 2300469248, 2319106048, 2303606784` |
| 2011 | 6 | `2348630016, 2304811008, 2309468160, 2300469248, 2324611072, 2305441792` |
| 2012 | 6 | `2348630016, 2304811008, 2311303168, 2300469248, 2330374144, 2313121792` |

RSS peaks increased over parts of this short window. These samples do not establish long-term growth or absence of growth. Existing calibration probes and prior terminal host OOM allocations are separate from expert recovery events.

## Independent audit and separate 128-seed supplement

Independent CPU audit of existing retained files: **exit0 / 3.0790271759033203 seconds**. Full audit SHA-256 `621c79241a83f8da62d0bfd55b149f3cfe26cb494ea34b66794c1153607c7436`; script SHA-256 `72a36c66470b98b6635a353777d457c81ac7fae4029d57f0e5fa4f1754821858`. It checked the model, twelve shards, consolidated ledger, natural report, retained own-job trajectory, source/LR split, KL, partial identities and final five expert intervals. An initial audit assumption equating attempted-type counts with valid counts failed on1999; the corrected audit preserves and independently reconciles the one invalid fault action. The failed attempt receipt is retained. No existing result was changed, and no GPU tests or evaluation reruns were performed.

## 88h 128-seed argmax / combat / card supplement — FINAL

All three modes reached a natural final state on the same retained `dev-88.pt` weights, SHA-256 `d12f4ad874a2bc0de7559aa76a3c1ed7525067622f19f8a94124f2c833abd13f`. Fixed seeds are **2000000000..2000000127**, with eight logical owners `seeds[owner::8]`. This is evaluation-only; the natural report records **0 parameter updates**. The ordinary1,024-game audit above was not repeated.

**Card: 作废：推演未随机化，偷看真实未来。** Its historical128/128 completion, raw mean33.1328125 and raw delta+3.3046875 (game-level paired SE0.9173898613433766) are retained as invalidated observations, not valid performance results. **Combat is FINAL at the shared one-hour cap**, with125/128 completed, `complete=false`, `skipped=combat_one_hour_cap`; its125-pair delta is **+1.688**, SE **0.9642256461919033**. The missing three combat seeds are final exclusions and will not be rerun.

These are fixed-seed comparisons for this snapshot. Combat's time-censored completed subset does not establish a full128-seed benefit. The two headline deltas use different intersections and should be compared using the common125-seed table below. No causal effect of the learning-rate cap or training changes is established.

### Natural final states and completed outcomes

| Mode | Completed /128 | Final status | Seconds | Mean floor | Act1 boss | Act2 boss | Act3 first | Act3 second flag | Heart |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| argmax | 128 | complete | 49.60093411401613 | 29.828125 | 106 | 26 | 11 | 3 | 0 |
| combat | 125 | FINAL: one-hour cap | 3600.34521674196 | 31.152 | 100 | 29 | 12 | 3 | 0 |
| card **作废** | 128 | complete | 435.64784919202793 | 33.1328125 | 111 | 40 | 15 | 6 | 0 |

For combat, the mean and progress counts refer to its125 completed episodes; all128 argmax and card episodes are included in their respective rows. Results retain the collector's `act3_win` label: argmax125death/3act3_win; combat122death/3act3_win; card122death/6act3_win, with0stalled and0Heart wins in each mode. These labels are not full-game victories or an exhaustive count of post-Act3 deaths.

| Mode | Ruby | Emerald | Sapphire | All3 keys | Act4 | Shield/Spear | Key/reach or boss contradictions |
| --- | --- | --- | --- | --- | --- | --- | --- |
| argmax | 6 | 24 | 10 | 1 | 0 | 0 | 0 |
| combat | 10 | 27 | 11 | 3 | 0 | 0 | 0 |
| card **作废** | 8 | 27 | 13 | 2 | 0 | 0 | 0 |

### Each mode versus argmax on its completed intersection

| Comparison | Pairs | Argmax mean | Mode mean | Paired delta | Paired SE | Excluded seeds |
| --- | --- | --- | --- | --- | --- | --- |
| combat vs argmax | 125 | 29.464 | 31.152 | +1.688 | 0.9642256461919033 | 2000000006, 2000000007, 2000000063 |
| card **作废** vs argmax | 128 | 29.828125 | 33.1328125 | +3.3046875 | 0.9173898613433766 | none |

Combat excludes **2000000006, 2000000007, 2000000063** because the shared mode wall cap was reached. These are outcome-dependent time-censoring exclusions, not zero-score games or evidence of improvement. The full128 argmax mean29.828125 must not be used as the baseline for the125 combat results; the matching baseline is **29.464**. Completed seeds are unique within their proper logical owner in every mode.

### Common three-mode intersection

The common intersection contains **125 seeds**, excluding the same three combat-censored seeds. All three modes below use exactly that common set.

| Mode | Mean floor | Act1 boss | Act2 boss | Act3 first | Act3 second flag | Heart | Delta vs argmax | Paired SE |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| argmax | 29.464 | 103 | 24 | 9 | 1 | 0 | 0 | 0 |
| combat | 31.152 | 100 | 29 | 12 | 3 | 0 | +1.688 | 0.9642256461919033 |
| card **作废** | 32.704 | 108 | 37 | 13 | 5 | 0 | +3.24 | 0.9299601101400063 |

The retained raw card-minus-combat difference on125 seeds is +1.552 (game-level paired SE1.0651945755921626), but **this comparison is also invalid under#156**. It is displayed only to identify and retract the earlier card-derived comparison, not as a performance conclusion.

### Attempted-work counters

| Mode | Mainline decisions | Preview decisions | Overrides | Preview steps | Card-reward decisions |
| --- | --- | --- | --- | --- | --- |
| argmax | 74978 | 0 | 0 | 0 | N/A |
| combat | 155219 | 66191 | 10697 | 44297709 | N/A |
| card **作废** | 84973 | 527 | 275 | 1925458 | 2188 |

These counters include all attempted work, including unfinished prefixes across natural windows; they are not restricted to the completed-pair subset. Card recorded **2188 card-reward decisions**, **527 previews**, **275 overrides**, and **1,925,458 preview steps**.

| Mode | Preview / mainline | Overrides / mainline | Overrides / previews |
| --- | --- | --- | --- |
| argmax | 0 | 0 | 0 |
| combat | 0.4264361965996431 | 0.0689155322479851 | 0.1616080736051729 |
| card **作废** | 0.006201970037541337 | 0.003236322125851741 | 0.5218216318785579 |

The intended #152 card preview protocol was: card_reward with>1 legal choice and argmax probability<.9; at most5 candidates including skip; the same8 determinize seeds paired; cloned ordinary argmax under the original mask, without combat or recursive preview. Each rollout stops at3 combats, death, the current-act boss, or1024 steps, whichever comes first. Selection compares ledger plus leafV (death0), and overrides only when paired mean advantage>SE. The counters above describe the historical GPU run. Under#156, the card previews did not actually randomize future trajectories at the reward screen, so the historical card exam is invalid despite those completed counts. No extra GPU tests or comparison runs were started.

### Shared mode caps and physical-worker timing

Each mode has its own shared3600-second cap across logical shards and natural allocations. For each natural window, the eight logical owners' times are **summed for each actual physical worker**, then the **maximum across physical workers** is taken. These window maxima are then summed. This includes initialization and completed-owner bookkeeping. Taking only the maximum of the eight logical-shard times would undercount serial work.

| Mode | Natural window token | Actual GPUs | Serial seconds by physical worker | Parallel maximum | Cumulative mode seconds |
| --- | --- | --- | --- | --- | --- |
| argmax | 1793948815846436250 | 6 | `0:49.60093411401613; 1:39.88602519698907; 2:25.803836759005208; 3:20.168610147025902; 4:20.34494599699974; 5:18.04939777700929` | 49.60093411401613 | 49.60093411401613 |
| combat | 1793948815846436250 | 6 | `0:337.3034017109894; 1:337.19452250393806; 2:337.0791505840025; 3:336.94324317597784; 4:336.94592467101756; 5:337.0219239940052` | 337.3034017109894 | 337.3034017109894 |
| combat | 1793949376795379087 | 5 | `0:460.35303447704064; 1:460.3465176479658; 2:460.35388245998183; 3:460.13737769494765; 4:460.04185509704985` | 460.35388245998183 | 797.6572841709713 |
| combat | 1793949978386812615 | 4 | `0:492.3537177990074; 1:492.3600944649661; 2:492.2744837239734; 3:492.28857174306177` | 492.3600944649661 | 1290.0173786359373 |
| combat | 1793950581330879653 | 3 | `0:2309.930779530958; 1:2309.9139404530288; 2:1801.7685125719872` | 2309.930779530958 | 3599.948158166895 |
| combat | 1793953000284294511 | 3 | `0:0.3970585750648752; 1:0.3884934380184859; 2:0.00017614499665796757` | 0.3970585750648752 | 3600.34521674196 |
| card **作废** | 1793953000284294511 | 3 | `0:411.0866125440225; 1:435.64784919202793; 2:183.01111790299183` | 435.64784919202793 | 435.64784919202793 |

All final timer receipts are present and final, with no failed prefix; `timing_complete=true` for every mode. Combat used actual worlds **6,5,4,3,3** and ended at **3600.34521674196 seconds**. Its preceding window ended at3599.948158166895 seconds, which was below the cap and was not prematurely marked final. The final natural window consumed the remaining allowance and produced the cap-final status. Card then naturally completed in the existing3-GPU allocation at **435.64784919202793 seconds**. A capped mode is final even though `complete=false`; the missing seeds are neither waited on nor retried.

### #156 invalidation and retained preview-SE request

Main instruction [#156](https://github.com/destire-mio/sts-silent-gpu/blob/556e2f7b8c0d2d31ad8e5d354872126e14de86c8/docs/messages/claude-to-gpt.md) invalidates card mode: `determinize` in `core/engine.inc` returns immediately outside combat. Card reward selection occurs after `room_in_combat` is cleared. The cloned previews therefore preserve the real run seed, and later `reseed_floor(g.run.seed, floor)` follows the real future. Calling the API eight times does not create eight randomized futures in this state. The label for the card results is **作废：推演未随机化，偷看真实未来**. The engine and the existing combat configuration are unchanged; proper whole-run randomization is awaiting separate Mac work.

The requested proportion of card previews with `paired_SE ==0` is **N/A: not computable from the retained files**. The recorded preview counter is **527**, but **0 individual card preview report records were retained**. A CPU-only recursive scan covered all91 files in `claude138-dev88` and the five related own-job logs5117/5130/5143/5150/5157 (96 files total). It found no persisted object with the card preview report's `candidates`, `preview_steps`, `depth=card` and `paired_SE` fields. Thus preview-report coverage is0/527; the numerator with SE0 is unknown, not measured0 and not inferred527.

The `paired_SE=0.9173898613433766` in the natural card mode summary is the SE of **128 completed games' floor differences**. It is not the SE of the eight trials within each card preview. Likewise, argmax's summary SE0 is a self-comparison and cannot answer the preview-level question.

Source inspection explains the retention gap: `CardRewardLookahead.select` creates per-preview reports in memory and returns them; `CardLookaheadSearch.predict` aggregates only preview/override/step counters. The periodic writer persists episodes, counters, snapshot hash, windows and preview RNG state, without the individual reports. The request explicitly prohibits a new GPU run; no replay was used to reconstruct missing values, and no100% statistic is fabricated.

The retained-file scan is `c166-preparation/card156-retained-record-scan.original.json`, SHA-256 `8d1fcca484effc2a521fd10d3873733b673c583a16d5fe34d4bb42e2595e09fe`. The original91 supplement files and prior publications are preserved. This narrow correction does not repeat the closed ordinary88 or supplement performance audit.

The temporary#156 scheduling change disabled card from96h. **#157 supersedes that disablement**: after merging `mac/run-determinize` (`52d21690cf57610864c5e8fa9e3ab60f61d388ee`), card previews alone pass `run=True`; training and combat calls retain the default behavior. The96h comparison is to run all three modes and record **card 已用整局随机化（#157）**. An isolated CUDA rebuild and the requested server `test_determinize`/`host_tests` are in progress; activation is pending their completion. Historical88h card remains invalid and will not be rerun. Actual96h behavior remains to be observed in its natural future run.

### Supplement audit and closure

Independent CPU audit: **exit0 / 0.6115639209747314 seconds**, reading **91 immutable existing files**:24 logical result records,56 window receipts,7 ready receipts,3 mode statuses and the final report. It independently checked snapshot hashes, per-owner seed uniqueness, outcome/key/reach consistency, all mode counters and fractions, physical-worker timing and each cumulative ready status, paired means/SEs, both mode-specific intersections and the common three-mode intersection. All91 file hashes were rechecked for stability. No model strict-load or ordinary88 audit was repeated; no GPU work, seed rerun, worker signal, checkpoint rewrite or forced save was performed.

The first audit attempt reused an older allowed-outcome list and stopped on the existing `act3_win` label. The corrected audit verifies that label against the Act3-second-boss/Heart flags and preserves the failed-attempt receipt. This was an audit-schema assumption, not a game or training failure.

- Natural `claude138-dev88/report.json`: **4861 bytes**, SHA-256 `05e8a78c837f6f5ad6fa964facb8f0540092b68d8e47172f7236399ae23659c9`.
- Full independent audit: `c165-preparation/supplement88.audit.original.json`, SHA-256 `ad3be1760244aca0f0ae941a673e43c147df296d849ff74e5f5db0a339be279f`.
- Audit script SHA-256: `0d13a686abb0185ba1c7a35935db900e0d97cc7571c29ed735df597bd4f268c0`; local summary/script and local/remote publication receipts are retained.

**The ordinary88 audit and historical supplement audit are CLOSED; card performance conclusions are INVALIDATED by#156.** No completed seed or capped combat exclusion is to be rerun. Natural training resumed afterward; update2013 was observed finite3GPU/24steps/0AMP/beta0/samples0/collectionfalse/student393216/fault0/current and nextLR4.444444444444445e-5. This is an observed complete row, not a claim that2013's Adam/collector was saved.

Next ordinary evaluation: **90h =324000 seconds**, strict LR window greater than **316912.1883957208**. The2012 training anchor Adam115159 has since been confirmed by canonical natural resumes (serials221–224), with no extra optimizer steps from repeated2012 saves. The88h partials **10 /3 /14 /26, total53, are already accounted and must not be added again at90h**. Next eight-hour supplement: **96h =345600 seconds, argmax/combat/card under #157**; card must use whole-run determinization after the requested CUDA build and server tests pass.
