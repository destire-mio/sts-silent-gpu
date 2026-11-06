# E4 90h evaluation — pure PPO, 学习率封顶 4.44e-5

The retained **dev-90** snapshot completed its ordinary **1,024-game argmax** exam at update **2072**, training time **324019.4058664447 seconds**, on **4 V100 GPUs**. Mean floor was **29.828125**, with **10 Act 3 second-boss flags** and **0 Heart wins**. Against dev-88 the paired difference was **-0.0498046875**, SE **0.3812564348838907**; against dev-70 it was **+0.478515625**, SE **0.39720421905983744**. These fixed-seed comparisons establish no causal effect of the learning-rate cap, engine changes, rollback, removing CE or GPU count.

## Protocol and model identity

- Seeds **2000000000..2000001023**, all 1,024 unique, exact ownership `seeds[rank::4]`.
- Ordinary argmax, no SL/search, observation version14, branch `dev78_rollback_pure_PPO`.
- Actual exam source **05fec51c18406c31a566d69db0424916ff546e79**, `c167-preparation/runtime`; build `/home/zhuyouwei/spire-build-e4-c157-run-determinize`.
- The #153 Pandora grid fix and #154 cap remain active. The #157 whole-run randomization opt-in applies only to card previews; training and combat retain default `run=False`. This ordinary exam is not a card-preview comparison.
- Metadata `learning_rate_cap=4.444444444444445e-5`, **学习率封顶 4.44e-5**.
- Model-only `dev-90.pt`: **49762853 bytes**, SHA-256 `38161dcff2ec1deaef01682df307a3bd8d833ab5293a7b452e3954af147cdac6`.
- Independent CPU strict load: CLS384 /6 layers /6 heads /1536 feedforward, **12,435,074 parameters**, all tensors finite.

The next canonical worker's small resume independently confirms natural training checkpoint **2072 / serial232 / Adam117096**, with rank match true and13 retained partial steps before the4→3 world change. This training-state counter includes retained unfinished-round work; it is not an Adam counter contained in the model-only dev-90 snapshot. No current large rank checkpoint was read.

## Full-game outcomes

| Metric | Count /1024 | Rate |
| --- | --- | --- |
| Act1 boss passed | 840 | 0.8203125 |
| Act2 boss passed | 199 | 0.1943359375 |
| Act3 first boss passed | 42 | 0.041015625 |
| Act3 second boss flag | 10 | 0.009765625 |
| Act4 entered | 0 | 0 |
| Shield/Spear passed | 0 | 0 |
| Heart victory | 0 | 0 |

Ruby / emerald / sapphire counts: **45 /165 /76**; all-three keys **4**. Independent key/reach and boss-hierarchy checks found **0 contradictions**.

Ledger labels are **1014 death,10 act3_win,0 stalled**. All games enter the mean and paired comparisons. The existing collector labels terminal episodes that passed the Act3 second boss without Heart as `act3_win` and removes death metadata. These flags are not full-game victories or an exhaustive count of post-Act3 deaths.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
| --- | --- | --- | --- |
| dev-88 | -0.0498046875 | 0.3812564348838907 | 1024 |
| dev-70 | +0.478515625 | 0.39720421905983744 | 1024 |
| dev-16 teacher argmax | +12.3583984375 | 0.3613772077930681 | 1024 |

## Act2 ledger audit

Consolidated `evaluation-2072-act2-ledger.json`: **1655678 bytes**, SHA-256 `854e14199fbaaa83e2929f0e41e4cae027d474fc981c0f958c4329b3d6c45cef`; **1024 episodes,840 Act2 entry snapshots**. Four result shards and four ledger shards were checked for unique seed ownership and agreement with the consolidated floors, keys, reach, normalized outcomes, stalled/death metadata, entry decks/relics/potions/HP/gold and counters.

- Card skips / removals / rests / upgrades: **1735 /2579 /3166 /2229**.
- Elites by Act1 /2 /3 /4: **1158 /765 /217 /0**.
- Mean Act2 entry deck size / HP / max HP / gold: **21.154761904761905 /58.23452380952381 /67.51428571428572 /180.73928571428573**.
- Tied-rank Spearman versus final floor: Act1 boss value **0.4286357436919077 (955 cases)**; Act2 entry value **0.4639783143370227 (840 cases)**.

## Retained training trajectory and learning rate

Strict window **(316912.1883957208,324019.4058664447]**: **60 complete pure PPO updates**, all beta0/samples0/collectionfalse, finite and0 AMP skips. Every segment retains the #153 Pandora grid fix. #156 source changes only future card scheduling; #157 adds card's whole-run randomization without opting training into it.

| Retained job | GPUs | Source | Complete updates | Rows | Complete applied steps | Engine/source phase |
| --- | --- | --- | --- | --- | --- | --- |
| 5157 | 3 | 48903cb | 2013–2051 | 39 | 936 | pandora_grid_153 |
| 5158 | 4 | cf958a5 | 2052–2055 | 4 | 128 | pandora_grid_153 |
| 5162 | 5 | cf958a5 | 2056–2058 | 3 | 120 | pandora_grid_153 |
| 5171 | 7 | 05fec51 | 2059–2066 | 8 | 448 | pandora_grid_153_with_run_determinize157 |
| 5186 | 5 | 05fec51 | 2067–2069 | 3 | 120 | pandora_grid_153_with_run_determinize157 |
| 5232 | 4 | 05fec51 | 2070–2072 | 3 | 96 | pandora_grid_153_with_run_determinize157 |

All current and next learning rates are **4.444444444444445e-5**, with **0 lower-bound touches**. KL min **0.006231065132596229**, max **0.010125909816955103**, ending **0.008198171427982848**. KL>.02 division by1.5 and the1e-5 lower bound are unchanged.

**1848 complete-row applied steps +89 retained partial steps once =1937 applied steps**, with0 AMP skips. The original canonical resume counters reconcile **117096−115159=1937**.

| Checkpoint / serial | Resume job | Adam before → after | Complete steps | Applied partial once |
| --- | --- | --- | --- | --- |
| 2051 /227 | 5158 | 115159 → 116107 | 936 | 12 |
| 2055 /228 | 5162 | 116107 → 116235 | 128 | 0 |
| 2058 /229 | 5171 | 116235 → 116389 | 120 | 34 |
| 2066 /230 | 5172 | 116389 → 116837 | 448 | 0 |
| 2069 /231 | 5232 | 116837 → 116987 | 120 | 30 |
| 2072 /232 | 5267 | 116987 → 117096 | 96 | 13 |

The new partial identities are **2051/227:12,2058/229:34,2069/231:30,2072/232:13**. The last13 are retained unfinished-round work associated with the natural2072 training checkpoint; they do not change which dev-90 model weights were evaluated. Repeated resumes of2058 or2066 are not added again. The old88 partials10/3/14/26 (53),86 partials1/21/24/38,84's22,82's7 and older3/30/16/49 remain in their earlier windows and must not enter90 or92 again.

Job5170's six-GPU orphan updates2059..2061, clocks322413.9785512827 /322537.2693743857 /322665.76595983875, contribute **144 excluded steps**. The retained seven-GPU redo2059..2066 is selected by persistence, source/world and exact logs, not maximum clock or duplicate update ID. Jobs5172 and5179 completed no new update before failure. Older orphan trajectories remain excluded.

Nominal student decisions **30,277,632 = Σ(N×2048×64)**; valid-mainline decisions **30,277,632**, difference0. Retained update fault counters and episode-fault records both total0 in this window. This does not guarantee long-term absence of faults. Update2072 contains524288 decisions: combat396305,card_reward13311,map25016,shop7793,campfire12362,other69501.

## Allocation failures and continuation

The original controller settled the already-notified failures once: job5170 host cgroup OOM (642s,6 GPUs,request7901MiB,481.5 node-s); jobs5172 and5179 FAILED with rank1 backward `CUBLAS_STATUS_ALLOC_FAILED` at `cublasCreate(handle)` (265s/198.75 and263s/197.25 node-s). The latter SIGKILLs on other ranks were torchrun cleanup; they are not evidence of host cgroup OOM. No MaxRSS is fabricated, and no failure is causally attributed to #157.

The original controller continued naturally, with no manual memory edit, refund, extra submission, forced save, worker signal, engine patch, replay or rebuild. The five-GPU5186 and four-GPU5232 allocations completed normally; three-GPU5267 resumed2072/232. The original controller alone settles accounting.

## Last five expert-round fields

Updates2068..2072 all explicitly log **beta0,samples0,collectionfalse**. **N/A means absent measurement while CE/expert collection is paused, not measured zero.** This preserves the70h field scope.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocation | Measured bytes/lane | Mem limit event | OOM recovered event |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2068 | 5 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2069 | 5 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2070 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2071 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2072 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurred in these five update intervals. Prior host/job failures and existing allocator probes are separate from expert recovery events.

| Update | v2 override labels | CE grad norm | PPO grad norm for CE merge | CE scale | Combat entropy |
| --- | --- | --- | --- | --- | --- |
| 2068 | N/A | N/A | N/A | N/A | 0.31164925146739314 |
| 2069 | N/A | N/A | N/A | N/A | 0.31357288633043207 |
| 2070 | N/A | N/A | N/A | N/A | 0.31382900916918854 |
| 2071 | N/A | N/A | N/A | N/A | 0.31328911851468333 |
| 2072 | N/A | N/A | N/A | N/A | 0.3102468900305271 |

Ordinary PPO/AMP/clipping continues while CE is paused. Minibatch means of gradient norms and scales must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
| --- | --- | --- |
| 2068 | 5 | `2321104896, 2298912768, 2291404800, 2314280960, 2286043136` |
| 2069 | 5 | `2322022400, 2304417792, 2293239808, 2314280960, 2288795648` |
| 2070 | 4 | `2294743040, 2290442240, 2328260608, 2273681408` |
| 2071 | 4 | `2326855680, 2329894912, 2356703232, 2304876544` |
| 2072 | 4 | `2327773184, 2329894912, 2359455744, 2304876544` |

Some peaks grow inside each short same-world segment; the world5→4 transition changes rank composition. These samples establish neither long-term growth nor its absence.

## Independent audit and closure

CPU-only audit of existing retained files: **exit0 /3.7079520225524902 seconds**. It strict-loads and checks the model, eight shards, consolidated ledger, natural report, explicit retained own-job logs, source/world/LR phases, student counts, KL, six canonical resume deltas and the final five expert intervals. No CUDA evaluation/test, seed rerun or current large rank read was performed.

- Full audit `c168-preparation/evaluation90.audit.original.json`: **203956 bytes**, SHA-256 `df9c05852267d4dd6b945ddb17966115f04e2765cf9318467d81ae0391da3181`.
- Audit script SHA-256 `d1eff3ba8524571adeb60f11bb40ec8f6f18ffdb2c3d485419938a62ea50485d`.
- A first driver start failed on the default Python's unsupported Path.write_text newline argument before any audit process started; its receipt is retained. The corrected driver ran the independent audit once.

Ordinary90 audit/extraction/publication is CLOSED. Next ordinary exam is92h (331200s), with the next LR window strictly after324019.4058664447; reuse the confirmed2072/232/Adam117096 anchor and do not add the89 partials again. The next128-seed supplement is96h (345600s), using argmax/combat/card and report note **card 已用整局随机化（#157）**. Each mode retains the shared3600s cap and natural time-censoring rules. Historical88 card remains invalid, **作废：推演未随机化，偷看真实未来**; no old seed or closed audit is repeated.
