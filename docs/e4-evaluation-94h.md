# E4 94h evaluation — pure PPO, 学习率封顶 4.44e-5

The retained **dev-94** snapshot completed its ordinary **1,024-game argmax** exam at update **2193**, training time **338403.48731481435 seconds**, on **7 V100 GPUs**. Mean floor was **30.40625**, with **11 Act3 second-boss flags** and **0 Heart wins**. Against dev-92 the paired difference was **+0.734375**, SE **0.391252550651588**; against dev-70 it was **+1.056640625**, SE **0.41462497109617036**. These fixed-seed comparisons establish no causal effect of the learning-rate cap, engine changes, rollback, removing CE or GPU count.

## Protocol and model identity

- Seeds **2000000000..2000001023**, all 1,024 unique, exact ownership `seeds[rank::7]`.
- Ordinary argmax, no SL/search, observation version14, branch `dev78_rollback_pure_PPO`.
- Actual exam source **05fec51c18406c31a566d69db0424916ff546e79**, `c167-preparation/runtime`; build `/home/zhuyouwei/spire-build-e4-c157-run-determinize`.
- The #153 Pandora grid fix and #154 cap remain active. #157 whole-run randomization applies only to card previews; training and combat retain default `run=False`. This ordinary exam does not compare card previews.
- Metadata `learning_rate_cap=4.444444444444445e-5`, **学习率封顶 4.44e-5**.
- Model-only `dev-94.pt`: **49762853 bytes**, SHA-256 `bdfb5d569fe74a10b53af2bfe1a7010e83945156f30b8ce3655b10e1227f48af`.
- Independent CPU strict load: CLS384 /6 layers /6 heads /1536 feedforward, **12,435,074 parameters**, all tensors finite.

The last real canonical resume read is job5322's **2192 /serial242 /Adam121975**, rank match true, retained partial0, new cursor0 across world8→7. The dev-94 snapshot is model-only at2193. **Adam122031 at2193 is derived arithmetic, not a saved counter read.** No current large rank checkpoint was read. The92 anchor Adam118672 at2136 has now been crossconfirmed using the later2141/237 canonical resume, its five subsequent complete updates and partial3; it is not a direct2136 counter read.

## Full-game outcomes

| Metric | Count /1024 | Rate |
| --- | --- | --- |
| Act1 boss passed | 853 | 0.8330078125 |
| Act2 boss passed | 219 | 0.2138671875 |
| Act3 first boss passed | 52 | 0.05078125 |
| Act3 second boss flag | 11 | 0.0107421875 |
| Act4 entered | 0 | 0 |
| Shield/Spear passed | 0 | 0 |
| Heart victory | 0 | 0 |

Ruby /emerald /sapphire counts: **29 /164 /24**; all-three keys **1**. Independent key/reach and boss-hierarchy checks found **0 contradictions**.

Ledger labels are **1013 death,11 act3_win,0 stalled**. All1,024 games enter the mean and paired comparisons. The existing collector labels terminal episodes that passed the Act3 second boss without Heart as `act3_win` and removes death metadata. These flags are not full-game victories or an exhaustive count of post-Act3 deaths.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
| --- | --- | --- | --- |
| dev-92 | +0.734375 | 0.391252550651588 | 1024 |
| dev-70 | +1.056640625 | 0.41462497109617036 | 1024 |
| dev-16 teacher argmax | +12.9365234375 | 0.38922601882795804 | 1024 |

## Act2 ledger audit

Consolidated `evaluation-2193-act2-ledger.json`: **1660694 bytes**, SHA-256 `297315b17921867f6915d842d77b8bdff1739d166b69ab7c6871cc84b6756ffd`; **1024 episodes,853 Act2 entry snapshots**. Seven result shards and seven ledger shards were checked for unique seed ownership and agreement with consolidated floors, keys, reach, normalized outcomes, stalled/death metadata, entry decks/relics/potions/HP/gold and counters.

- Card skips /removals /rests /upgrades: **2651 /2579 /3234 /2332**.
- Elites by Act1 /2 /3 /4: **1190 /771 /236 /0**.
- Mean Act2 entry deck size /HP /max HP /gold: **20.91559202813599 /57.975381008206334 /67.51699882766705 /186.9495896834701**.
- Tied-rank Spearman versus final floor: Act1 boss value **0.38079330100634873 (957 cases)**; Act2 entry value **0.44311057633694423 (853 cases)**.

## Retained training trajectory and learning rate

Strict window **(331221.9739686065,338403.48731481435]**: **57 complete pure PPO updates**, all beta0/samples0/collectionfalse and finite. There were **2 AMP skipped steps**, both in update2173; that row records62 applied steps rather than64. This is separate from the episode-fault counter, which is0. All retained allocations use source05fec51, the #153 Pandora grid fix, #154 cap and #157 build. Training does not opt into card's whole-run randomization.

| Retained job | GPUs | Source | Complete updates | Rows | Complete applied steps | AMP skips | Engine/source phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 5274 | 4 | 05fec51 | 2137–2141 | 5 | 160 | 0 | pandora_grid_153_with_run_determinize157 |
| 5312 | 5 | 05fec51 | 2142–2144 | 3 | 120 | 0 | pandora_grid_153_with_run_determinize157 |
| 5319 | 6 | 05fec51 | 2145–2152 | 8 | 384 | 0 | pandora_grid_153_with_run_determinize157 |
| 5321 | 8 | 05fec51 | 2153–2192 | 40 | 2558 | 2 | pandora_grid_153_with_run_determinize157 |
| 5322 | 7 | 05fec51 | 2193 | 1 | 56 | 0 | pandora_grid_153_with_run_determinize157 |

All current and next learning rates are **4.444444444444445e-5**, with **0 lower-bound touches**. KL min **0.0077637270407396874**, max **0.012146477884554398**, ending **0.00900267700865596**. KL>.02 division by1.5 and the1e-5 lower bound are unchanged.

**3278 complete-row applied steps +81 retained partial steps once =3359 applied steps**, with2 AMP skips. Original canonical resumes reconcile the following retained lineage:

| Checkpoint /serial | Resume job | Adam before → after | Complete applied steps | Applied partial once |
| --- | --- | --- | --- | --- |
| 2141 /237 | 5312 | 118672 → 118835 | 160 | 3 |
| 2144 /238 | 5319 | 118835 → 118991 | 120 | 36 |
| 2152 /239 | 5321 | 118991 → 119417 | 384 | 42 |
| 2192 /242 | 5322 | 119417 → 121975 | 2558 | 0 |

Through2192 the real counter difference is **121975−118672=3303=3222+81**. Update2193 adds56 applied steps, giving **derived Adam122031** and a window delta3359. The two skipped steps at2173 explain why job5321's40 complete rows applied2558 rather than2560 steps.

New partial identities **2141/237:3,2144/238:36,2152/239:42** are included once in94. The failed5320 and normal5321 read the same2152/239 input; its42 partial steps are not added twice. **5320 has no complete new update;2153 belongs to5321.** The92 partial2132/236:8 and90 partials2051/227:12,2058/229:34,2069/231:30,2072/232:13 were already counted in their windows and are excluded here. Earlier partials and historical orphan trajectories remain excluded. Periodic2167/2181 saves and final2192 save do not duplicate work. All57 retained rows, including2142 and2173, were read; no unread round was extrapolated.

Nominal student decisions **53,739,520 = Σ(N×2048×64)**; valid-mainline decisions **53,739,520**, difference0. Retained update fault counters and episode-fault records both total0 in this window. This does not guarantee long-term absence of faults. Update2193 contains917504 decisions: combat693473,card_reward23154,map44104,shop12824,campfire22276,other121673.

## Continuation and allocation failure

Job5321 completed normally after5436seconds on8 GPUs; the original controller settled **5436 node-seconds** once. Job5322 naturally resumed on7 GPUs. At the coordination read,5339 was its own7-GPU dependent successor; no manual allocation action occurred.

The already reported5320 host cgroup OOM remains a single closed failure:8 GPUs, request11854MiB,283seconds and283 node-seconds, original terminal record `5320|OUT_OF_MEMORY|0:125|283|2026-11-06T22:16:36|2026-11-06T22:21:19`. The chain records one oom-kill and rank1 SIGKILL, with other ranks cleaned up by torchrun; CUDA allocator warnings also exist. MaxRSS was unavailable. This was not a single-game fault and establishes no causal link to#157. Original `c169-preparation/oom5320.observation.json`:23332bytes, SHA-256 `fa63ef841717030c81173b583c386c24705f1812a1db4668fec4930176233c4f`. It was not re-settled, replayed or re-notified. No manual memory edit, refund, extra submission, forced save, worker signal, engine patch or rebuild occurred.

## Last five expert-round fields

Updates2189..2193 all explicitly log **beta0,samples0,collectionfalse**. **N/A means absent measurement while CE/expert collection is paused, not measured zero.** This preserves the70h field scope.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocation | Measured bytes/lane | Mem limit event | OOM recovered event |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2189 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2190 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2191 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2192 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2193 | 7 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurred in these five update intervals. Existing allocator probes and host/job failures are separate from expert recovery events.

| Update | v2 override labels | CE grad norm | PPO grad norm for CE merge | CE scale | Combat entropy |
| --- | --- | --- | --- | --- | --- |
| 2189 | N/A | N/A | N/A | N/A | 0.32320697058472203 |
| 2190 | N/A | N/A | N/A | N/A | 0.32396402048007367 |
| 2191 | N/A | N/A | N/A | N/A | 0.3254665181316053 |
| 2192 | N/A | N/A | N/A | N/A | 0.3236911340512945 |
| 2193 | N/A | N/A | N/A | N/A | 0.3245221952111106 |

Ordinary PPO/AMP/clipping continues while CE is paused. Minibatch means of gradient norms and scales must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
| --- | --- | --- |
| 2189 | 8 | `2508242944, 2371887104, 2392608768, 2386305024, 2372820992, 2403733504, 2421256192, 2407309312` |
| 2190 | 8 | `2508242944, 2371887104, 2392608768, 2386305024, 2372820992, 2403733504, 2421256192, 2407309312` |
| 2191 | 8 | `2508242944, 2371887104, 2392608768, 2386305024, 2372820992, 2403733504, 2421256192, 2407309312` |
| 2192 | 8 | `2508242944, 2371887104, 2392608768, 2386305024, 2372820992, 2403733504, 2421256192, 2407309312` |
| 2193 | 7 | `2271211520, 2276732928, 2290335744, 2271797248, 2275577856, 2280497152, 2300768256` |

The four eight-GPU observations have unchanged reported peaks; the world8→7 transition changes rank composition. These short samples establish neither long-term growth nor its absence.

## Independent audit and closure

CPU-only audit of existing retained files: **exit0 /3.5252692699432373 seconds**. It strict-loads and checks the model, fourteen shards, consolidated ledger, natural report, explicit retained own-job logs, source/world/LR phases, student counts, KL, original canonical resume deltas and the final five expert intervals. The actual independent audit process ran **once**, with empty stderr. An initial transport decompression failure occurred before any audit process; its local driver error was retained. No CUDA evaluation/test, seed rerun or current large rank read was performed.

- Full audit `c170-preparation/evaluation94.audit.original.json`: **204609 bytes**, SHA-256 `f81b0dc5bf80dabf4843ca1452e2ce54f92e5f22bcaeef4f5d28232dfe88fdbc`.
- Audit script SHA-256 `571687360401fdc6c150b65929a9e837be755732b96972bdbcd1ebbf4fc3faab`.
- Source job logs were hashed as read. Job5322 remains active, so its log hash identifies the audit-time file and is not a promise of an immutable live log.

| Artifact | Bytes | SHA-256 |
| --- | --- | --- |
| evaluation-2193-rank-0.json.gz | 1464 | `97af1dbb929462e44b40bfee8deccbc3e61dade505795447abf1af9cb4397fd0` |
| evaluation-2193-ledger-rank-0.json.gz | 16906 | `306ba1c23b1088866ae079183df6956a81a90014b7b2169e05d2e8a5fbc77e23` |
| evaluation-2193-rank-1.json.gz | 1481 | `d896749663a53d0e2fa64aef05362ff57c4ee421293b41a43e87b6b112df9c12` |
| evaluation-2193-ledger-rank-1.json.gz | 16845 | `4c0d173a8a10deb396e0eae626aa6dad3901eb048cb5dc8dab0763ad4f87c96c` |
| evaluation-2193-rank-2.json.gz | 1440 | `db2ed8b691811c1e977f36ce213085b7bce074d1dc889ba040e393a953556d30` |
| evaluation-2193-ledger-rank-2.json.gz | 16312 | `9649e2b567ab69c343c168e7277c54bce99f829f34573de23119316553b2f5de` |
| evaluation-2193-rank-3.json.gz | 1407 | `0615aa5ad051a9e19e7bfc89375b075ac400ee214157d3e1adc618abdbebfb5f` |
| evaluation-2193-ledger-rank-3.json.gz | 16668 | `a6d712f228f4a94ec63dea78d5028cafe88b00ee796ccf676e8a60b2730de3b7` |
| evaluation-2193-rank-4.json.gz | 1404 | `a686793122fee6e404cc494fbf4d33ac5bef0f2f365a689af907906de0492f2c` |
| evaluation-2193-ledger-rank-4.json.gz | 16412 | `88120c25ebafb5df427c980cefdc6d4c7ead522fea862571e476b7521bf53772` |
| evaluation-2193-rank-5.json.gz | 1449 | `57e273a624c7fe2215bc4ad4c1eb90d3a48a84f7c2deced3d51c5b88f480dc43` |
| evaluation-2193-ledger-rank-5.json.gz | 16015 | `de60d2c28a81de85c9cf3a6e287133164fa9d975353120148a3b3cfa69fe688e` |
| evaluation-2193-rank-6.json.gz | 1419 | `e100265c2d6059bb9ca74b221d58606a1d9319c9922958cdfd9dc28e6507f187` |
| evaluation-2193-ledger-rank-6.json.gz | 16511 | `40d0d4a8b320262cd2bef87e95bf24d7e91916344f88406a69db0222ec708809` |

Ordinary94 audit/extraction/publication is CLOSED. Next ordinary exam is96h (345600s), with the next LR window strictly after338403.48731481435. Last real counter remains2192/242/Adam121975;2193/Adam122031 is derived until a real later canonical resume confirms it. Do not add the94 partial81 or any older partials again. The next128-seed supplement is96h (345600s), using argmax/combat/card and report note **card 已用整局随机化（#157）**. Each mode retains the shared3600s cap and natural time-censoring rules. Historical88 card remains invalid, **作废：推演未随机化，偷看真实未来**; no old seed or closed audit is repeated.
