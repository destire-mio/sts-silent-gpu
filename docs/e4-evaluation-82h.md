# E4 82h evaluation — dev-78 rollback, pure PPO

The retained **dev-82** checkpoint completed the ordinary **1,024-game argmax** exam at update **1823**, training time **295231.8622608829 seconds**, on **5 V100 GPUs**. Mean floor was **29.740234375**, with **7 Act 3 victories** and **0 Heart wins**. The paired difference versus dev-80 was **+1.6982421875**, SE **0.39356118729836426**; versus dev-70 it was **+0.390625**, SE **0.4191782084790545**.

These are fixed-seed comparisons, not a causal estimate of the rollback or removing expert CE. Weights, training time, GPU counts and retained optimizer state also differ.

## Evaluation protocol and checkpoint identity

- Seeds: **2000000000..2000001023**, all 1,024 unique, assigned as `seeds[rank::5]`.
- Policy: argmax, no SL/search; observation/build **v14**.
- Training branch: **from dev-78 rollback, pure PPO, 5 GPUs at the evaluated update**.
- Runtime source: `a8dd2210939be2049fd254acb15c0fc588a246fe` (`c155-preparation/runtime`), code publication `cb5ccc9f3d23e5e2679a99cf66f0b60319a53c06`.
- Model snapshot: `dev-82.pt`, **49,762,853 bytes**, SHA-256 `fa11124d3e4e79f2e767627e04b8300acd751499b4fffa66d57734e73316a3ce`.
- Independent CPU load: strict CLS configuration **384 / 6 layers / 6 heads / 1536 feedforward**, **12,435,074 parameters**, all tensors finite. This snapshot contains model weights only.

The one-time dev-78 weight rollback retained Adam, scaler, calibration coefficient, pools, RNG and training clock. Expert collection and CE are paused. The dev-80 supplemental comparison used its independently retained dev-80 weights; it performed no training updates and has already been published separately in the 80h report.

## Full-game outcomes

| Metric | Count / 1024 | Rate |
|---|---:|---:|
| Act 1 boss passed | 831 | 0.8115234375 |
| Act 2 boss passed | 196 | 0.19140625 |
| Act 3 first boss passed | 40 | 0.0390625 |
| Act 3 victory | 7 | 0.0068359375 |
| Heart victory | 0 | 0 |

Outcomes were **1015 deaths, 7 Act 3 wins and 2 stalled games**. All 1,024 episodes remain in the reported mean and paired comparisons. There were **0 progression/key contradictions**, **0 three-key completions**, **0 Act 4 entries**, **0 Shield/Spear wins**, and **0 Heart wins**. Ruby/emerald/sapphire key counts were **12 / 168 / 17**. An Act 3 victory is not a Heart victory.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
|---|---:|---:|---:|
| dev-80 | +1.6982421875 | 0.39356118729836426 | 1024 |
| dev-70 | +0.390625 | 0.4191782084790545 | 1024 |
| dev-16 teacher argmax | +12.2705078125 | 0.36788894186201937 | 1024 |

## Act 2 ledger audit

The consolidated `evaluation-1823-act2-ledger.json` contains all **1,024 episodes**, including **831 Act 2 entry snapshots**. It is **1,645,433 bytes**, SHA-256 `ca7fe6ec27a3d4ec790e23499812d9f4ef2663d6c5837c028cb4ac70e11512ac`.

Five result shards and five ledger shards were checked independently for exact seed ownership. Their consolidated per-game floors, keys, reach flags, outcomes, stalled markers, death metadata and counters agree.

- Card skips / removals / rests / upgrades: **1832 / 2497 / 3052 / 2285**.
- Elites by Act 1 / 2 / 3 / 4: **1238 / 844 / 211 / 0**.
- Mean Act 2 entry deck size / HP / max HP / gold: **21.21179302045728 / 58.10589651022864 / 67.95908543922984 / 184.27436823104694**.
- Tied-rank Spearman against final floor: Act 1 boss value **0.43233306105210273**, **955 cases**; Act 2 entry value **0.48862595485231214**, **831 cases**.

## Retained training window and optimizer accounting

The learning-rate window is strictly **(288169.9435521559, 295231.8622608829]**. Every retained completed update in this window is `dev78_rollback_pure_PPO`; earlier old-v2 allocations spent on the independent dev-80 comparison produced no additional completed training updates in this window. Allocation wall time must not be confused with the retained training clock.

| Retained job | GPUs | Complete updates | Updates | Applied steps in complete rows | AMP skips |
|---|---:|---|---:|---:|---:|
| 5047 | 3 | 1762–1818 | 57 | 1368 | 0 |
| 5048 | 4 | 1819–1822 | 4 | 128 | 0 |
| 5049 | 5 | 1823–1823 | 1 | 40 | 0 |

There are **62 completed updates**, **1536 applied optimizer steps in complete rows**, and **0 AMP skips**. Add **7 already-applied partial steps once**, retained at checkpoint **1818 / serial 196** when moving from 3 to 4 GPUs, for **1543 total applied steps**. The new world cleared the unfinished cursor; those seven steps are not included in the following full update row.

The retained Adam counters independently cross-check this:

- 80h anchor **108113** → 3-to-4-GPU resume **109488**: **1368 complete steps + 7 retained partial**.
- 4-to-5-GPU resume **109616**: another **128 complete steps**, **0 new partial**.
- Implied Adam step at the evaluated update: **109656**.
- The later 5-to-6-GPU resume counter **109736** also includes **80 post-evaluation steps from updates 1824–1825**; those belong to the next window.

Previously accounted partials **3 / 30 / 16 / 49** were not added again.

Learning rate ranged from **4.4444444444444447e-5** to **6.666666666666667e-5**, ending at **6.666666666666667e-5**, with **0 lower-bound touches**. KL ranged from **0.0049724278448345** to **0.013688324453944477**, ending at **0.013688324453944477**.

The evaluated training update contained **655360 student decisions = 5 × 2048 × 64**: combat **472215**, card reward **17515**, map **35544**, shop **12583**, campfire **13985**, other **103518**. Expert labels were **0**. Training completions were **1221 fresh** and **1172 midstart episodes**, with midstart mean floor **36.592150170648466**; these are training-round counts, not the fixed 1,024 full-game exam.

## Last five expert-round fields

Updates **1819–1822** used four GPUs and **1823** used five. Every row explicitly logs `expert_beta=0`, `expert_samples=0` and `expert_collection=false`. The usual expert columns are retained below; **N/A means the subsystem was paused and that measurement was not produced**, rather than a measured zero.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 1819 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1820 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1821 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1822 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1823 | 5 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

No `expert_lane_memory_limit` or `expert_oom_recovered` event occurred in these five update intervals.

| Update | Override labels | Mean CE gradient norm | Mean PPO gradient norm for CE merge | Mean CE scale | Combat entropy |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1819 | 0 | N/A | N/A | N/A | 0.3119475111877315 |
| 1820 | 0 | N/A | N/A | N/A | 0.3036909450118265 |
| 1821 | 0 | N/A | N/A | N/A | 0.2974438364382313 |
| 1822 | 0 | N/A | N/A | N/A | 0.29750423115540525 |
| 1823 | 0 | N/A | N/A | N/A | 0.30526440037108576 |

The CE-merge gradient measurements are N/A because CE is paused. Ordinary PPO/AMP/clipping continues; these missing expert measurements are not zero-gradient claims.

| Update | GPUs | Per-rank RSS peaks, bytes |
|---:|---:|---|
| 1819 | 4 | `2019606528, 2032582656, 2034966528, 2020118528` |
| 1820 | 4 | `2019606528, 2043957248, 2040471552, 2020118528` |
| 1821 | 4 | `2019606528, 2043957248, 2040471552, 2020118528` |
| 1822 | 4 | `2019606528, 2043957248, 2040471552, 2020118528` |
| 1823 | 5 | `2301534208, 2297966592, 2288619520, 2263482368, 2326835200` |

This is a short observation window across two GPU counts, not evidence that memory cannot grow over longer runs.

## Training episode-fault records

The retained training logs contain **two episode faults** after updates **1802** and **1804**, both for seed **1341088830**, reason `engine_fault:2639331021`. They are distinct fresh/midstart episodes with different candidate paths. The failed probe marks its parent episode stalled; it is not a win or a training-job terminal failure. Subsequent updates remain finite. These training incidents are separate from the two stalled outcomes in the fixed-seed evaluation.

| After update | Start kind | Parent slot | Probe slot | Candidate count | Randomization (path position, seed) |
|---:|---|---:|---:|---:|---|
| 1802 | fresh | 1313 | 4233 | 944 | None |
| 1804 | act2_boss | 23 | 3121 | 936 | `(791, 3879899490884761920)` |

The original records, including the newly logged `randomizations` and `start_kind`, are retained unchanged:

- [Fresh episode record](faults/episode-fault5047-seed1341088830-after1802.json): **3029 bytes**, SHA-256 `28be02c0ba72326045aa8d36d1c1340505a3afdbf916edd9a7fbd12ba192d71f`.
- [Midstart episode record](faults/episode-fault5047-seed1341088830-after1804.json): **3040 bytes**, SHA-256 `50853d2719bea948e9aab87d0b1f2560318986364c54cec58e1800c630963453`.

The 64-bit randomization seed is preserved as exact decimal text in the records. No replay, engine/capacity change or extra GPU test was performed for these incidents; the underlying engine assertion remains undiagnosed here.

## Audit artifacts and continuation

Independent CPU audit artifact SHA-256: `0cb40ff76b574d6b0b907935b77923928e7b74d4b7b6a0aecfb401d189f4603e`; audit script SHA-256: `a2b1193c7082bc97bbc324eeaa2b9131318470be8eeef7e683b9bf3ceb186f38`.

No evaluation games were rerun. This ordinary 82h boundary has **no additional 128-seed combat comparison**. Next ordinary evaluation: **84h = 302400 seconds**, with the learning-rate window strictly greater than **295231.8622608829**. The next 8h supplemental comparison is **88h = 316800 seconds**. The seven partial steps are now accounted in this 82h window and must not be added again to 84h.
