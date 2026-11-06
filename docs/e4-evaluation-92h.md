# E4 92h evaluation — pure PPO, 学习率封顶 4.44e-5

The retained **dev-92** snapshot completed its ordinary **1,024-game argmax** exam at update **2136**, training time **331221.9739686065 seconds**, on **4 V100 GPUs**. Mean floor was **29.671875**, with **7 Act 3 second-boss flags** and **0 Heart wins**. Against dev-90 the paired difference was **-0.15625**, SE **0.3805654238760415**; against dev-70 it was **+0.322265625**, SE **0.421120712432703**. These fixed-seed comparisons establish no causal effect of the learning-rate cap, engine changes, rollback, removing CE or GPU count.

## Protocol and model identity

- Seeds **2000000000..2000001023**, all 1,024 unique, exact ownership `seeds[rank::4]`.
- Ordinary argmax, no SL/search, observation version14, branch `dev78_rollback_pure_PPO`.
- Actual exam source **05fec51c18406c31a566d69db0424916ff546e79**, `c167-preparation/runtime`; build `/home/zhuyouwei/spire-build-e4-c157-run-determinize`.
- The #153 Pandora grid fix and #154 cap remain active. #157 whole-run randomization applies only to card previews; training and combat retain default `run=False`. This ordinary exam does not compare card previews.
- Metadata `learning_rate_cap=4.444444444444445e-5`, **学习率封顶 4.44e-5**.
- Model-only `dev-92.pt`: **49762853 bytes**, SHA-256 `368cbd5069bc3824511aa796a3cedfdb0242168fb5d499035c1dd0f887800bea`.
- Independent CPU strict load: CLS384 /6 layers /6 heads /1536 feedforward, **12,435,074 parameters**, all tensors finite.

The last real canonical resume read is job5274's **2132 / serial236 / Adam118544**, rank match true, retained partial8, new cursor0 across world3→4. The dev-92 model is a model-only snapshot at2136. **Adam118672 at2136 is derived arithmetic, not a saved counter read.** No current large rank checkpoint was read.

## Full-game outcomes

| Metric | Count /1024 | Rate |
| --- | --- | --- |
| Act1 boss passed | 826 | 0.806640625 |
| Act2 boss passed | 194 | 0.189453125 |
| Act3 first boss passed | 39 | 0.0380859375 |
| Act3 second boss flag | 7 | 0.0068359375 |
| Act4 entered | 0 | 0 |
| Shield/Spear passed | 0 | 0 |
| Heart victory | 0 | 0 |

Ruby / emerald / sapphire counts: **39 /139 /70**; all-three keys **4**. Independent key/reach and boss-hierarchy checks found **0 contradictions**.

Ledger labels are **1016 death,7 act3_win,1 stalled**. All1,024 games enter the mean and paired comparisons. The existing collector labels terminal episodes that passed the Act3 second boss without Heart as `act3_win` and removes death metadata. These flags are not full-game victories or an exhaustive count of post-Act3 deaths.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
| --- | --- | --- | --- |
| dev-90 | -0.15625 | 0.3805654238760415 | 1024 |
| dev-70 | +0.322265625 | 0.421120712432703 | 1024 |
| dev-16 teacher argmax | +12.2021484375 | 0.3750396526076552 | 1024 |

## Act2 ledger audit

Consolidated `evaluation-2136-act2-ledger.json`: **1631253 bytes**, SHA-256 `6b193809a812b7727e1052272af9afc08f6ac20a821e0a1b79c95bfb747b449c`; **1024 episodes,826 Act2 entry snapshots**. Four result shards and four ledger shards were checked for unique seed ownership and agreement with consolidated floors, keys, reach, normalized outcomes, stalled/death metadata, entry decks/relics/potions/HP/gold and counters.

- Card skips / removals / rests / upgrades: **2493 /2419 /3180 /2220**.
- Elites by Act1 /2 /3 /4: **1219 /790 /216 /0**.
- Mean Act2 entry deck size / HP / max HP / gold: **20.96489104116223 /57.94794188861985 /67.5363196125908 /186.9273607748184**.
- Tied-rank Spearman versus final floor: Act1 boss value **0.47020435001400157 (962 cases)**; Act2 entry value **0.510785033989264 (826 cases)**.

## Retained training trajectory and learning rate

Strict window **(324019.4058664447,331221.9739686065]**: **64 complete pure PPO updates**, all beta0/samples0/collectionfalse, finite and0 AMP skips. Both retained allocations use source05fec51, the #153 Pandora grid fix, #154 cap and #157 build. Training does not opt into card's whole-run randomization.

| Retained job | GPUs | Source | Complete updates | Rows | Complete applied steps | Engine/source phase |
| --- | --- | --- | --- | --- | --- | --- |
| 5267 | 3 | 05fec51 | 2073–2132 | 60 | 1440 | pandora_grid_153_with_run_determinize157 |
| 5274 | 4 | 05fec51 | 2133–2136 | 4 | 128 | pandora_grid_153_with_run_determinize157 |

All current and next learning rates are **4.444444444444445e-5**, with **0 lower-bound touches**. KL min **0.005750095493019747**, max **0.010395352547344139**, ending **0.008277138815528141**. KL>.02 division by1.5 and the1e-5 lower bound are unchanged.

**1568 complete-row applied steps +8 retained partial steps once =1576 applied steps**, with0 AMP skips. Job5274's original canonical resume reconciles **118544−117096=1448=1440+8** through2132. The next four complete rows add128, so the derived Adam at2136 is118672; this is not a claim that a2136 training-state counter was saved or read.

| Checkpoint / serial | Resume job | Adam before → after | Complete steps | Applied partial once |
| --- | --- | --- | --- | --- |
| 2132 /236 | 5274 | 117096 → 118544 | 1440 | 8 |

The new partial identity **2132/236:8** is included once in this92h window. The old90 partials **2051/227:12,2058/229:34,2069/231:30,2072/232:13** were already counted in90 and are not added again. Earlier-window partials also remain excluded. Periodic small saves2088/2104/2121 and the final2132 save do not duplicate work. All64 retained rows, including2073, were read in this audit; no unread round was extrapolated. Historical orphan trajectories remain excluded.

Nominal student decisions **25,690,112 = Σ(N×2048×64)**; valid-mainline decisions **25,690,112**, difference0. Retained update fault counters and episode-fault records both total0 in this window. This does not guarantee long-term absence of faults. Update2136 contains524288 decisions: combat393992,card_reward13757,map25374,shop6875,campfire13514,other70776.

## Continuation

Job5267 completed normally after6955seconds on3 GPUs; the original controller settled **2608.125 node-seconds** once. Job5274 naturally resumed on4 GPUs and retained the new8 applied partial steps with cursor reset0. No manual memory edit, refund, extra submission, forced save, worker signal, engine patch, replay or rebuild occurred. No new allocation failure was found in this retained window; old failures remain closed.

## Last five expert-round fields

Updates2132..2136 all explicitly log **beta0,samples0,collectionfalse**. **N/A means absent measurement while CE/expert collection is paused, not measured zero.** This preserves the70h field scope.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocation | Measured bytes/lane | Mem limit event | OOM recovered event |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2132 | 3 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2133 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2134 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2135 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2136 | 4 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurred in these five update intervals. Existing allocator probes and host/job failures are separate from expert recovery events.

| Update | v2 override labels | CE grad norm | PPO grad norm for CE merge | CE scale | Combat entropy |
| --- | --- | --- | --- | --- | --- |
| 2132 | N/A | N/A | N/A | N/A | 0.2940736135073585 |
| 2133 | N/A | N/A | N/A | N/A | 0.2946837388216973 |
| 2134 | N/A | N/A | N/A | N/A | 0.285409251172998 |
| 2135 | N/A | N/A | N/A | N/A | 0.288636061705631 |
| 2136 | N/A | N/A | N/A | N/A | 0.28703341186061676 |

Ordinary PPO/AMP/clipping continues while CE is paused. Minibatch means of gradient norms and scales must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
| --- | --- | --- |
| 2132 | 3 | `2459279360, 2371850240, 2363748352` |
| 2133 | 4 | `2304696320, 2279792640, 2321965056, 2314506240` |
| 2134 | 4 | `2335891456, 2311905280, 2354077696, 2353958912` |
| 2135 | 4 | `2335891456, 2313740288, 2354077696, 2353958912` |
| 2136 | 4 | `2335891456, 2313740288, 2354077696, 2353958912` |

Some peaks grow inside the short four-GPU segment; the world3→4 transition changes rank composition. These samples establish neither long-term growth nor its absence.

## Independent audit and closure

CPU-only audit of existing retained files: **exit0 /8.154548168182373 seconds**. It strict-loads and checks the model, eight shards, consolidated ledger, natural report, explicit retained own-job logs, source/world/LR phases, student counts, KL, the canonical resume delta and the final five expert intervals. The independent audit ran **once**, with empty stderr. No CUDA evaluation/test, seed rerun or current large rank read was performed.

- Full audit `c169-preparation/evaluation92.audit.original.json`: **209466 bytes**, SHA-256 `8fdf37ca03db08fa7a95e6abef6a8df25e5f887b8ad1468977d01e6fdcb4516c`.
- Audit script SHA-256 `8b272dea8917f3581201afe58686b9b5282be7f5099c969c5a8961de6787d6a8`.
- Source job logs were hashed as read. Job5274 remains active, so its log hash identifies the audit-time file and is not a promise of an immutable live log.

| Artifact | Bytes | SHA-256 |
| --- | --- | --- |
| evaluation-2136-rank-0.json.gz | 2124 | `651b165d2f0a7fe3fc09c72225b62ff1cb6708d6aec906674ac7139b314bcdfd` |
| evaluation-2136-ledger-rank-0.json.gz | 26471 | `2ac152a157b54b84a9e57cf972feafcddbd67c55f016a1be0bc949d052e0bae3` |
| evaluation-2136-rank-1.json.gz | 2090 | `c72fa9de77de4b6b1d1e3b195facbd3e61fe8b142685835dad3bd2aa3443d12d` |
| evaluation-2136-ledger-rank-1.json.gz | 28048 | `b0a2a5b8f7b8c886aae44a5423501eef519d0ca202dff355a5983e15c6fcdd67` |
| evaluation-2136-rank-2.json.gz | 2158 | `3fb6f188187d09cd552a39849a8bbdcf6f907ee61cbd838858c7fd706dc5e9e2` |
| evaluation-2136-ledger-rank-2.json.gz | 27035 | `68da3ba35e6418e8dcfd92b203c52d0472b1b9023137e11c494f7520fefcef3b` |
| evaluation-2136-rank-3.json.gz | 2169 | `9e7cc32e1f79ead49deb4cc8d3e379056927da233a68a5906a31b24447ed138a` |
| evaluation-2136-ledger-rank-3.json.gz | 27751 | `bb192d60c6b29345338bf1c02bd460c1ba92cd5704fe55da46fc4be36e7d4927` |

Ordinary92 audit/extraction/publication is CLOSED. Next ordinary exam is94h (338400s), with the next LR window strictly after331221.9739686065. Last real counter remains2132/236/Adam118544;2136/Adam118672 is derived until a real later canonical resume confirms it. Do not add the new8 partials or any older partials again. The next128-seed supplement is96h (345600s), using argmax/combat/card and report note **card 已用整局随机化（#157）**. Each mode retains the shared3600s cap and natural time-censoring rules. Historical88 card remains invalid, **作废：推演未随机化，偷看真实未来**; no old seed or closed audit is repeated.
