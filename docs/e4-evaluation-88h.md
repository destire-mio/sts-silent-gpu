# E4 88h evaluation — pure PPO, 学习率封顶 4.44e-5

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

Updates **1952–1991** remained at **6.666666666666667e-5**. The new source began at **1992**, with current and next LR **4.444444444444445e-5** through2012. The old segment is not described as capped. KL>.02 division by1.5 and the lower bound1e-5 remain unchanged. There were **0 lower-bound touches**. KL ranged **0.0058187438349556935..0.012449484183708087**, ending at **0.008444850111367364**.

There are **1880 complete-row applied optimizer steps**, plus **53 already-applied partial steps once**: **10 + 3 + 14 + 26 = 1933 total applied steps**, with **0 AMP skips**.

| Checkpoint / serial | World transition | Verified Adam at resume | Delta | Complete steps in delta | Applied partial once |
| --- | --- | --- | --- | --- | --- |
| 1991 / 215 | 3→4 | 114196 | 970 from86 anchor113226 | 960 | 10 |
| 1995 / 216 | 4→5 | 114324 | 128 | 128 | 0 |
| 1998 / 217 | 5→6 | 114447 | 123 | 120 | 3 |
| 2001 / 218 | 6→5 | 114605 | 158 | 144 | 14 |
| 2004 / 219 | 5→7 | 114751 | 146 | 120 | 26 |
| 2007 / 220 | 7→6 | 114919 | 168 | 168 | 0 |

The first resume also cross-checks Adam114196−112554 =1642 =68 complete three-GPU updates1924–1991 (1632 steps) +10 partial; only40 of those updates lie in this88h window. Through2007, **1693 =1640 complete +53 partial** is directly verified by successor resume counters. Five subsequent six-GPU rows2008–2012 contribute240 complete steps, implying Adam **115159** at the evaluated update. This final counter is not claimed to be read or persisted.

The new partial identities **1991/215:10, 1998/217:3, 2001/218:14, 2004/219:26** are counted once globally. Old86 partials1/21/24/38,84 partial22,82 partial7 and older3/30/16/49 remain in their earlier windows. They must not be added here or again at90h. Retained lineage, not maximum clocks among duplicate IDs or repeated rank/job logs, selects the rows. Old5050/5052 orphans stay excluded.

Nominal student decisions are **30,801,920 = Σ(N × 2048 × 64)**; valid-mainline decisions are **30,801,919**. The one-action difference is from the new fault below. Decision-type counts count attempted actions, including that invalid fault action; valid-mainline excludes it. Update2012 has **786432 =6 ×2048 ×64**: combat599772, card_reward19756, map36550, shop10621, campfire18712, other101021.

## Newly retained training episode fault

Job **5114**, after update **1998** and before1999, recorded seed **1180012898**, reason **`engine_fault:1078467217`**, parent slot **1837**, probe slot **null**, start kind **`act2_boss`**, with **1468 candidate actions**. This occurred on actual source489/c162 with the Pandora build. Its root cause has not been established; it is not evidence that the older `contains(index)` defect recurred.

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

At the bounded status read during this audit, the **128-seed supplement remains in progress**: argmax is complete (**49.60093411401613 seconds**), combat is not complete (**337.3034017109894 accumulated seconds**, no cap-skip), card and final report are not yet present. These are observations of the existing natural workflow, not a completed comparison. No partial-result gain is claimed.

Argmax/combat/card each have a shared3600-second mode cap. Eight logical shards are assigned across actual physical workers: serial shard wall times sum per worker, then parallel maximum per natural window, then sum across windows. A capped mode is FINAL even with missing seeds; completed seeds are never rerun. Final reporting must show each mode's completed intersection versus argmax, the common three-mode intersection and exclusions; time censoring is not an improvement.

Card preview remains evaluation-only: card_reward with>1 legal choice and argmax probability<.9; at most5 candidates including skip; same8 determinize seeds paired; cloned ordinary argmax with original masks, no combat or recursive preview. Each rollout stops at3 combats/death/current-act boss/1024 steps, whichever comes first; score ledger plus leafV (death0); override only if paired mean advantage>SE. Report card decisions, preview decisions, overrides and preview steps from actual files when final.

The ordinary88h audit and publication are closed by this report; the supplement is tracked separately until its natural FINAL report. Next ordinary evaluation: **90h =324000 seconds**, strict LR window greater than **316912.1883957208**. The88h partials **10 /3 /14 /26 are now accounted and must not be added again at90h**. The next eight-hour supplement is96h; completion of the current88h supplement remains pending.
