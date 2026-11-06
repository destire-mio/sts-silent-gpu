# E4 96h ordinary evaluation — pure PPO, 学习率封顶 4.44e-5

The retained **dev-96** snapshot completed its ordinary **1,024-game argmax** exam at update **2251**, training time **345695.23689094814 seconds**, on **7 V100 GPUs**. Mean floor was **30.6142578125**, with **10 Act3 second-boss flags** and **0 Heart wins**. Against dev-94 the paired difference was **+0.2080078125**, SE **0.37887817048727923**; against dev-70 it was **+1.2646484375**, SE **0.4080041929303037**. These fixed-seed comparisons establish no causal effect of the learning-rate cap, engine changes, rollback, removing CE or GPU count.

This report closes the ordinary exam. The separate96h128-seed argmax/combat/card supplement was still running at the coordination read and is not finalized here. Its card protocol note is **card 已用整局随机化（#157）**. Completed seeds are not rerun; each mode retains its shared3600-second cap and natural censoring rules.

## Protocol and model identity

- Seeds **2000000000..2000001023**, all1,024 unique, exact ownership `seeds[rank::7]`.
- Ordinary argmax, no SL/search, observation version14, branch `dev78_rollback_pure_PPO`.
- Actual exam source **05fec51c18406c31a566d69db0424916ff546e79**, `c167-preparation/runtime`; build `/home/zhuyouwei/spire-build-e4-c157-run-determinize`.
- #153 Pandora grid fix and #154 cap remain active. #157 whole-run randomization applies only to card previews; training and combat retain default `run=False`. This ordinary exam does not compare previews.
- Metadata `learning_rate_cap=4.444444444444445e-5`, **学习率封顶 4.44e-5**.
- Model-only `dev-96.pt`: **49762853 bytes**, SHA-256 `53bebb9f05ae21b64c9d538e2740a933a1cfc565d18a69defba7d852c05878dc`.
- Independent CPU strict load: CLS384 /6 layers /6 heads /1536 feedforward, **12,435,074 parameters**, all tensors finite.

The last real canonical resume read remains job5322's **2192 /serial242 /Adam121975**, rank match true, retained partial0, new cursor0 across world8→7. **The94 anchor Adam122031 at2193 and96 endpoint Adam125278 at2251 are both derived arithmetic, not saved counter reads.** The94 anchor uses the real2192 counter plus the previously audited2193 row's56 applied steps. No current large rank checkpoint was read.

## Full-game outcomes

| Metric | Count /1024 | Rate |
| --- | --- | --- |
| Act1 boss passed | 876 | 0.85546875 |
| Act2 boss passed | 213 | 0.2080078125 |
| Act3 first boss passed | 48 | 0.046875 |
| Act3 second boss flag | 10 | 0.009765625 |
| Act4 entered | 2 | 0.001953125 |
| Shield/Spear passed | 1 | 0.0009765625 |
| Heart victory | 0 | 0 |

Ruby /emerald /sapphire counts: **42 /154 /46**; all-three keys **3**. Independent key/reach and boss-hierarchy checks found **0 contradictions**.

Two seeds entered Act4 with all keys. Seed **2000000183** ended at floor55 without passing Shield/Spear; seed **2000000268** ended at floor56 after passing Shield/Spear. Both have Heart=false.

Ledger labels are **1014 death,10 act3_win,0 stalled**. All1,024 games enter the mean and paired comparisons. The existing collector labels terminal episodes that passed the Act3 second boss without Heart as `act3_win` and removes death metadata. These flags are not full-game victories or an exhaustive count of post-Act3 deaths.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
| --- | --- | --- | --- |
| dev-94 | +0.2080078125 | 0.37887817048727923 | 1024 |
| dev-70 | +1.2646484375 | 0.4080041929303037 | 1024 |
| dev-16 teacher argmax | +13.14453125 | 0.36599574032126025 | 1024 |

## Act2 ledger audit

Consolidated `evaluation-2251-act2-ledger.json`: **1689687 bytes**, SHA-256 `2a3a3fa557782755c21ce8796017bbc2e378235e3a8118ee1f1b727fe79d2881`; **1024 episodes,876 Act2 entry snapshots**. Seven result shards and seven ledger shards were checked for unique seed ownership and agreement with consolidated floors, keys, reach, normalized outcomes, stalled/death metadata, entry decks/relics/potions/HP/gold and counters.

- Card skips /removals /rests /upgrades: **2614 /2580 /3208 /2341**.
- Elites by Act1 /2 /3 /4: **1151 /799 /226 /2**.
- Mean Act2 entry deck size /HP /max HP /gold: **20.932648401826484 /58.31963470319635 /67.51826484018265 /185.162100456621**.
- Tied-rank Spearman versus final floor: Act1 boss value **0.43415101557006414 (976 cases)**; Act2 entry value **0.43214176011043376 (876 cases)**.

## Retained training trajectory and learning rate

Strict window **(338403.48731481435,345695.23689094814]**: **58 complete pure PPO updates**, all from retained job5322 on7 GPUs, source05fec51/c167/newbuild, #153 engine fix, #154 cap and #157 build. All58 rows are finite, beta0/samples0/collectionfalse. Training does not opt into card's whole-run randomization.

| Retained job | GPUs | Source | Complete updates | Rows | Complete applied steps | AMP skips | Engine/source phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 5322 | 7 | 05fec51 | 2194–2251 | 58 | 3247 | 1 | pandora_grid_153_with_run_determinize157 |

Update **2244** records **55 applied steps and1 AMP skip**; the other57 rows record56 applied steps each. Thus **58×56−1=3247 complete applied steps**, plus **0 new retained partial steps**, equals **3247 applied steps** for this96h window. The skip is separate from the episode-fault counter, which is0.

All current and next learning rates are **4.444444444444445e-5**, with **0 lower-bound touches**. KL min **0.007386758208766576**, max **0.009944750352999838**, ending **0.008915303838650452**. KL>.02 division by1.5 and the1e-5 lower bound are unchanged.

The real2192/242 counter121975 plus the previously audited2193 row56 gives derived94 anchor122031. Adding the58 newly audited rows3247 gives **derived Adam125278 at2251**. The3303 steps since the real2192 counter include2193 and must not all be charged to96. No later saved counter was read; periodic small saves2206/2221/2236 do not add work.

The94 partial identities **2141/237:3,2144/238:36,2152/239:42** (81 total),92 partial2132/236:8,90 partials2051/227:12,2058/229:34,2069/231:30,2072/232:13 (89 total) and all earlier partials remain counted only in their original windows. Update2193 belongs to94 and is excluded from96. Historical orphan trajectories remain excluded. All58 retained rows2194..2251 were read; no unread round was extrapolated.

Nominal student decisions **53,215,232 = Σ(N×2048×64)**; valid-mainline decisions **53,215,232**, difference0. Retained update fault counters and episode-fault records both total0 in this window. This does not guarantee long-term absence of faults. Update2251 contains917504 decisions: combat692592,card_reward23104,map44284,shop13103,campfire21826,other122595.

## Continuation

At the coordination read, job5322 was still the actual7-GPU worker, with its own7-GPU dependent successor5339 pending. Latest training update2251 remained visible while natural supplementary evaluation was running. There was no manual memory edit, refund, extra submission, forced save, worker signal, engine patch, replay or rebuild. Previously reported5320 and older allocation failures remain closed; none is newly attributed to this ordinary window.

## Last five expert-round fields

Updates2247..2251 all explicitly log **beta0,samples0,collectionfalse**. **N/A means absent measurement while CE/expert collection is paused, not measured zero.** This preserves the70h field scope.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocation | Measured bytes/lane | Mem limit event | OOM recovered event |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2247 | 7 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2248 | 7 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2249 | 7 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2250 | 7 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2251 | 7 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurred in these five update intervals. Existing allocator probes and host/job failures are separate from expert recovery events.

| Update | v2 override labels | CE grad norm | PPO grad norm for CE merge | CE scale | Combat entropy |
| --- | --- | --- | --- | --- | --- |
| 2247 | N/A | N/A | N/A | N/A | 0.33920793471683874 |
| 2248 | N/A | N/A | N/A | N/A | 0.3379583134380999 |
| 2249 | N/A | N/A | N/A | N/A | 0.3378713140632527 |
| 2250 | N/A | N/A | N/A | N/A | 0.33619259550237285 |
| 2251 | N/A | N/A | N/A | N/A | 0.335968931195482 |

Ordinary PPO/AMP/clipping continues while CE is paused. Minibatch means of gradient norms and scales must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
| --- | --- | --- |
| 2247 | 7 | `2525900800, 2338439168, 2350338048, 2332471296, 2334875648, 2345820160, 2360254464` |
| 2248 | 7 | `2525900800, 2338439168, 2350338048, 2332471296, 2334875648, 2345820160, 2360254464` |
| 2249 | 7 | `2525900800, 2338439168, 2350338048, 2332471296, 2334875648, 2345820160, 2360254464` |
| 2250 | 7 | `2525900800, 2338439168, 2350338048, 2332471296, 2334875648, 2345820160, 2360254464` |
| 2251 | 7 | `2525900800, 2338439168, 2350338048, 2332471296, 2334875648, 2345820160, 2360254464` |

Reported peaks are unchanged across these five observations on the same7-GPU worker. These short samples establish neither long-term growth nor its absence.

## Independent audit and closure

CPU-only audit of existing retained files: **exit0 /3.607215642929077 seconds**. It strict-loads and checks the model, fourteen shards, consolidated ledger, natural report, explicit retained own-job logs, source/world/LR phases, student counts, KL, original canonical resume input and the final five expert intervals. The independent audit process ran **once**, with empty stderr. No CUDA evaluation/test, seed rerun or current large rank read was performed.

- Full audit `c171-preparation/evaluation96.audit.original.json`: **203991 bytes**, SHA-256 `7f92321b62407f06dfb7de56c0c85a67ab8d6562a719c1d40ad63402ea74543e`.
- Audit script SHA-256 `2b0dcd772791b59d254774cff85eb941ace345b82fb15c5f383aa98c30053c81`.
- Job5322's log was hashed as read: **288403 bytes**, SHA-256 `fdf5afbb2ec8a9f39f8161c314e30cce3b8990a0ff71c923de104ad5a9b8fa29`. It remains active; this identifies the audit-time file and is not a promise of an immutable live log.

| Artifact | Bytes | SHA-256 |
| --- | --- | --- |
| evaluation-2251-rank-0.json.gz | 1456 | `cd6f16c25cd0c0d0149db35abfbe0b31a65566732c532d789526e2104898c534` |
| evaluation-2251-ledger-rank-0.json.gz | 17123 | `3acd3f87a6b8e648151f5858f1fa00bf59bf3ac0f7838486adf8d457cffc096e` |
| evaluation-2251-rank-1.json.gz | 1445 | `10bfff777ccada3edfaf34d8477c375ec2264d0e5e159dc1e44494972447406c` |
| evaluation-2251-ledger-rank-1.json.gz | 17712 | `1c8a8f613f6e528f461b56df43dc8081b67246b357ad4b578dcb0370e6ddee3c` |
| evaluation-2251-rank-2.json.gz | 1427 | `4a7afc6176e10debf9ac392f57695eb7127b85d27ad8e6635e6438515889d882` |
| evaluation-2251-ledger-rank-2.json.gz | 15891 | `113163c1f1ac61d91ff2d2148d7f502ad45f8de056ca8626749a3dec886014e2` |
| evaluation-2251-rank-3.json.gz | 1458 | `fe3936f68014036baab78a950d2a3b7ccfd3cf67068209374d3e6a39b8e8b131` |
| evaluation-2251-ledger-rank-3.json.gz | 16733 | `e75ff4a8e5220658b9fbd5122c54c5bc2efb384832092794c337efe1d84ee596` |
| evaluation-2251-rank-4.json.gz | 1430 | `041b88e4ef41e206dff4bb5140c0e63d0f7e5ee53b81159df3e27a46f1460b4d` |
| evaluation-2251-ledger-rank-4.json.gz | 17146 | `3c8a88dd312f60e1bf272b2eaa6e71168fff0f325a10b828026637f0899442ae` |
| evaluation-2251-rank-5.json.gz | 1469 | `86a6575e7f548710a561b17fec11d155b6cc82a309b18cfe00cd803d488f513d` |
| evaluation-2251-ledger-rank-5.json.gz | 16934 | `428fe1d16395585477aa02423d25b68c0eb8ae33e6ddbbb1efad831faefde12b` |
| evaluation-2251-rank-6.json.gz | 1438 | `2d37bfd3e3864e07c9a97fd2769bcde2cbf0d1dd707a263b8a6ceed1a1a8f964` |
| evaluation-2251-ledger-rank-6.json.gz | 17447 | `6cdafd108e906606b370791952a92de27f62081ec3455d4a0a3e9fd8f28c5e9b` |

Ordinary96 audit/extraction/publication is CLOSED. Next ordinary exam is98h (352800s), with the next LR window strictly after345695.23689094814. Last real counter remains2192/242/Adam121975;2251/Adam125278 is derived until a real later canonical resume confirms it. No new96 partial has been added. Keep all94/92/90 and older partials excluded from later windows.

The96h128-seed supplement remains OPEN pending natural FINAL, with argmax/combat/card and **card 已用整局随机化（#157）**. Each mode retains its shared3600s cap; report completed intersections and exclusions, without treating censored games as gains or rerunning seeds. Historical88 card remains invalid, **作废：推演未随机化，偷看真实未来**. No old closed audit or seed is repeated.
