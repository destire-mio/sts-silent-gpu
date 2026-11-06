# E4 84h evaluation — dev-78 rollback, pure PPO

The retained **dev-84** weights completed the ordinary **1,024-game argmax** exam at update **1887**, training time **302444.91937079903 seconds**, on **3 V100 GPUs**. Mean floor was **29.0263671875**, with **12 Act 3 victory flags** and **0 Heart wins**. The paired difference versus dev-82 was **−0.7138671875**, SE **0.4222113547794548**; versus dev-70 it was **−0.3232421875**, SE **0.4164550799633647**.

One seed reached Act 4 and passed Shield/Spear; it did not win against the Heart. These fixed-seed comparisons do not establish a causal effect of removing CE, rolling back weights, GPU count, or the pending Pandora engine fix. **This exam still used the old v14 engine.**

## Evaluation protocol and checkpoint identity

- Seeds: **2000000000..2000001023**, all 1,024 unique, exact ownership `seeds[rank::3]`.
- Selection: argmax, no SL/search; observation/build **v14**.
- Branch: **from dev-78 rollback, pure PPO**, evaluated on 3 GPUs.
- Actual exam source: `c289fa22c33ce22adac1545ef520ceb823af8411`, `c158-preparation/runtime`, publication `eb1a673a566caec1cc54984bda812305cf16e47e`. The card lookahead addition is evaluation-only and has not been used in this ordinary exam or in training.
- Snapshot: `dev-84.pt`, **49,762,853 bytes**, SHA-256 `839a46ae6b351bbce8a00f37afc3bc5bfd2d0c8393dfd2c1b52b0816825ce034`.
- Independent CPU strict load: CLS **384 / 6 layers / 6 heads / 1536 feedforward**, **12,435,074 parameters**, all tensors finite. This file contains model weights only.

The evaluated model snapshot and complete report are retained. The last training-state checkpoint observed during this audit was update **1883**; a later live update is not automatically a saved optimizer/collector checkpoint. The Adam value at the evaluated update below is explicitly **implied from the verified trajectory**, not read from a large current rank checkpoint.

## Full-game outcomes

| Metric | Count / 1024 | Rate |
|---|---:|---:|
| Act 1 boss passed | 791 | 0.7724609375 |
| Act 2 boss passed | 189 | 0.1845703125 |
| Act 3 first boss passed | 47 | 0.0458984375 |
| Act 3 second boss passed / victory flag | 12 | 0.01171875 |
| Act 4 entered | 1 | 0.0009765625 |
| Shield/Spear passed | 1 | 0.0009765625 |
| Heart victory | 0 | 0 |

Result/ledger outcome labels are **1010 `death`, 12 `act3_win`, and 2 `stalled`**; all 1,024 remain in the mean and paired comparisons. The existing collector labels any terminal episode that passed the Act 3 second boss without a Heart win as `act3_win`, and removes its death metadata (`train/e4_evaluation_ledger.py`). Thus this label also covers later non-Heart endings; it is not a full-game win or an exhaustive count of deaths after Act 3.

The Act 4 case is seed **2000000120**, final floor **56**, with all three keys, Act 3 second-boss / Act 4 / Shield-Spear flags true, and Heart false. Ruby / emerald / sapphire counts were **32 / 215 / 230**, and **9** episodes finished with all three keys. Independent checks found **0 progression/key contradictions** and **0 boss-hierarchy violations**.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
|---|---:|---:|---:|
| dev-82 | −0.7138671875 | 0.4222113547794548 | 1024 |
| dev-70 | −0.3232421875 | 0.4164550799633647 | 1024 |
| dev-16 teacher argmax | +11.556640625 | 0.3879705322382605 | 1024 |

## Act 2 ledger audit

The consolidated `evaluation-1887-act2-ledger.json` contains **1,024 episodes**, including **791 Act 2 entry snapshots**; **1,600,647 bytes**, SHA-256 `dd14652f352bdf741ffb517f57f8c89c8092a4ffd8ea3b4e0429938b5ff02a9c`.

Three result shards and three ledger shards were independently checked for seed ownership and agreement with the consolidated floors, keys, reach flags, normalized outcomes, stalled markers, available death metadata, entry decks/relics/potions/HP/gold and counters.

- Card skips / removals / rests / upgrades: **2091 / 2360 / 2819 / 2404**.
- Elites by Act 1 / 2 / 3 / 4: **1378 / 847 / 248 / 1**.
- Mean Act 2 entry deck size / HP / max HP / gold: **21.217446270543615 / 57.51706700379267 / 67.83185840707965 / 193.48546144121366**.
- Tied-rank Spearman against final floor: Act 1 boss value **0.46052678666909425** (**924 cases**); Act 2 entry value **0.45071232451439047** (**791 cases**).

## Training window, source phases and optimizer accounting

The learning-rate window is strictly **(295231.8622608829, 302444.91937079903]**. Every complete update reflected in these evaluated weights is `dev78_rollback_pure_PPO`: beta 0, no expert collection or CE. Both source phases used the old v14 build. The future `5ac291b` / Pandora build was not active in this exam.

| Retained job | GPUs | Source | Complete updates | Updates | Complete-row steps | AMP skips |
|---|---:|---|---|---:|---:|---:|
| 5049 | 5 | a8dd221 | 1824–1825 | 2 | 80 | 0 |
| 5051 | 6 | a8dd221 | 1826–1826 | 1 | 48 | 0 |
| 5053 | 5 | a8dd221 | 1827–1829 | 3 | 120 | 0 |
| 5054 | 4 | c289fa2 | 1830–1833 | 4 | 128 | 0 |
| 5067 | 3 | c289fa2 | 1834–1887 | 54 | 1296 | 0 |

There are **64 complete updates**, **1672 applied optimizer steps in complete rows**, and **0 AMP skips**. Add **22 already-applied partial steps once**, at checkpoint **1829 / serial 200**, when moving from 5 to 4 GPUs: **1694 total applied steps**. The unfinished cursor was cleared; the 22 steps are not included in the subsequent complete row. Earlier partials **7 / 3 / 30 / 16 / 49** belong to earlier reports and were not added again.

Verified resume counters cross-check the retained path:

- 82h anchor **109656** → checkpoint1825 resume **109736**: **80 steps**.
- → checkpoint1826 resume **109784**: **48 steps**, using job5051's redone update.
- → checkpoint1829 resume **109926**: **120 complete steps + 22 applied partial**.
- → checkpoint1833 resume **110054**: **128 steps**.
- Job5067 then contributes **54 × 24 = 1296** complete steps through1887, implying **111350** at the evaluated update. This final counter is not asserted to have been saved in a current rank checkpoint.

**OOM orphan rows were excluded, regardless of their higher clocks:**

- Job5050's unsaved1826 at **295598.2367799579**, **48 steps**, was replaced by job5051's persisted1826 at **295594.2703572009**.
- Job5052's unsaved1827–1829 at **295726.3196865229 / 295851.26311661093 / 295977.01827005594**, **56 steps each / 168 total**, were replaced by job5053's five-GPU recomputation.

Do not choose the maximum clock among duplicate update IDs or count retained partials again for each successor job. Slurm charges for failed allocations remain real and were settled once by the original controller; this optimizer accounting does not refund or recharge GPU time.

LR was constant **6.666666666666667e−5**, with **0 lower-bound touches**. KL ranged **0.005033684537670872..0.014815919585763403**, ending at **0.00835399151876152**.

Nominal student decisions over this window were **27,394,048 = Σ(N × 2048 × 64)**; the logged valid-mainline total was **27,394,047**. The one-decision difference is the invalid fault action in update1860 (**393215 valid vs393216 nominal**). It was retained explicitly rather than silently forcing the formula. The evaluated update1887 contains **393216 = 3 × 2048 × 64** decisions: combat299459, card_reward9828, map18192, shop5437, campfire9303, other50997.

## Last five expert-round fields

Updates **1883–1887** all used 3 GPUs and explicitly logged `expert_beta=0`, `expert_samples=0`, `expert_collection=false`. **N/A means the paused subsystem did not produce the measurement; it is not a measured zero.**

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| 1883 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1884 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1885 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1886 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 1887 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurred in these five update intervals.

| Update | Override labels | Mean CE gradient norm | Mean PPO gradient norm for CE merge | Mean CE scale | Combat entropy |
|---:|---:|---:|---:|---:|---:|
| 1883 | 0 | N/A | N/A | N/A | 0.31149193442218365 |
| 1884 | 0 | N/A | N/A | N/A | 0.3115403675873324 |
| 1885 | 0 | N/A | N/A | N/A | 0.31027345425598246 |
| 1886 | 0 | N/A | N/A | N/A | 0.3081815262211773 |
| 1887 | 0 | N/A | N/A | N/A | 0.3090857190365431 |

CE-merge norm and scale are N/A while CE is paused; ordinary PPO/AMP/clipping continues. Even when present, minibatch-mean norm and scale must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
|---:|---:|---|
| 1883 | 3 | `2471989248, 2352152576, 2364047360` |
| 1884 | 3 | `2472570880, 2353070080, 2364047360` |
| 1885 | 3 | `2472570880, 2353070080, 2364047360` |
| 1886 | 3 | `2472570880, 2353070080, 2364047360` |
| 1887 | 3 | `2472570880, 2353070080, 2364047360` |

This short RSS window does not prove absence of longer-term growth. The previously reported host OOM allocations are separate from expert-lane OOM recovery events.

## Training episode fault and pending engine transition

The retained window contains **one training episode fault**, after update1859: seed **1547193734**, reason `engine_fault:2639331021`, start kind `act2`, parent slot1176, probe4083, **764 candidates**. The randomization is **(353, 8794227798865536230)**; the 64-bit seed is preserved as exact decimal text in the original record.

[Original fault record](faults/episode-fault5067-seed1547193734-after1859.json): **2517 bytes**, SHA-256 `f75a3827cf7ff95867b0222426af048e5e1caaec2508424a2b1596ecfc9de8ee`.

This is the already-reported old-engine incident, separate from the **two stalled evaluation games**. It is not a victory or a terminal training-job failure. Subsequent updates were finite. No new replay or extra GPU test was run for it.

The Pandora/CallingBell `grid_source=Grid` fix from Mac commit `2e8a69569e4a2144b3c94a3d480bcdc5d8867d17` was merged as `aa91f4bc6d8c997b76788cc72681c8463b9e422e`, fully rebuilt, and passed144 server host tests. Its source `5ac291b10e47ecbe386c6a67789692678c4a887c` and new build are future-bound. Actual new-engine source, five CUDA gates, fresh collectors/four pools, first finite update and the naturally persisted one-time marker remain pending. This exam cannot validate that GPU fix.

## Audit artifacts and continuation

Independent CPU audit: **exit0 / 4.159457445144653 seconds**. Full audit SHA-256 `f244cfee7ae96566cec8b0a5b1e4ec79b785143d7a2c42ce036947672b19884d`; script SHA-256 `ee7c97b9fb005acf86cf238609f9a6b70f69f012c874131fc4e9973d30d6660c`. The audit loads existing retained files only; no evaluation games were rerun.

This ordinary84h boundary has **no 128-seed supplement**. Next ordinary evaluation: **86h = 309600 seconds**, LR window strictly greater than **302444.91937079903**. Next8h supplement: **88h = 316800 seconds**, with separately capped argmax/combat/card modes, unchanged completion-intersection and physical-worker timing rules. The **22 partial steps are now accounted in84h and must not be added again to86h**.
