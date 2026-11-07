# E4 ordinary evaluation at 102 training hours

The natural argmax evaluation completed all 1024 fixed seeds at update **2417**, training clock **367230.0037447115**, on job **5342 / 8 V100 GPUs**, source `a7213984f4eb9717b9ae3e350368a14b2e8aff15` (`c175`), using the existing #157 build. Mean floor is **30.9677734375**. The paired difference from 100h is **+0.326171875**, SE **0.3775434307009639**; Heart wins are **0**.

This is the first ordinary examination after #159 actually took effect at update2380/362435.6291895434. The training window includes both the preceding source and #159, so this comparison does not establish an LR, engine or midstart-randomization cause. The 98h and 100h examinations preceded #159. Claude decides any future 768-model switch; this result does not trigger one automatically. #160/#163 remain preparation only, without a live student or future binding change.

## Evaluation and retained model

Fixed seeds are2000000000..2000001023. Eight result shards and eight ledger shards each contain exactly their rank::8 seed partition, with no missing or duplicate seeds. The stalled case remains in the 1024-case mean and every paired comparison. Selection is ordinary argmax, observation version14, no SL, dev78-rollback pure PPO, 384-wide CLS model with6 layers/6 heads/FF1536, **12435074 parameters**. Strict CPU loading and all retained weight tensors being finite passed.

| Metric | Count / value |
| --- | --- |
| Episodes | 1024 |
| Mean floor | 30.9677734375 |
| act1_boss | 855/1024 (0.8349609375) |
| act2_boss | 244/1024 (0.23828125) |
| act3_first_boss | 52/1024 (0.05078125) |
| Act3 second-boss flag | 10 |
| Heart | 0 |
| Ruby / Emerald / Sapphire keys | 69 / 168 / 51 |
| All three keys | 11 |
| Act4 entries | 2 |
| Shield/Spear wins | 2 |

The two Act4 cases, seeds **2000000268** and **2000000140**, both finish at floor56, have all three keys and Shield/Spear true, and Heart false. Key/reach contradictions and boss-hierarchy violations are both **0**.

| Comparison | Paired mean floor difference | Paired SE | Cases |
| --- | --- | --- | --- |
| 102h minus100h | 0.326171875 | 0.3775434307009639 | 1024 |
| 102h minus dev70 | 1.6181640625 | 0.39780960296291823 | 1024 |
| 102h minus dev16 teacher | 13.498046875 | 0.3803584204935841 | 1024 |

| Artifact | Bytes | SHA-256 |
| --- | --- | --- |
| dev-102.pt | 49768265 | `f316b18a4695eff98ea5a20cbef51c3a1422c0b571e9e1e552016ed6ccb9477f` |
| evaluation-2417-act2-ledger.json | 1648967 | `8bdd7190f78bbc827eea469b901bf72448295a342be5e3d852e2f11b7a6e427e` |

`dev-102.pt` contains model weights, not Adam or a training cursor.

## Ledger and value checks

The consolidated ledger exactly matches all eight retained ledger shards and each result's floor, keys and reach. Outcomes are **1013 death / 10 act3_win / 1 stalled**. The collector's act3_win label denotes the Act3 second-boss flag with death details removed; it is not a full-game victory and does not enumerate all subsequent Act3 deaths.

All **855** Act2 entries retain deck, relics, potions, HP/max HP and gold, with valid card upgrade flags. Death records retain room type, encounter, room-entry HP, remaining potions and turn; placeholder potion slots are excluded.

| Ledger field | Aggregate |
| --- | --- |
| card_skips | 2742 |
| card_removals | 2751 |
| rests | 3254 |
| upgrades | 2308 |

| Act | Elites |
| --- | --- |
| 1 | 1132 |
| 2 | 749 |
| 3 | 244 |
| 4 | 2 |

| Act2 entry field | Mean |
| --- | --- |
| deck_size | 20.555555555555557 |
| hp | 58.528654970760236 |
| max_hp | 67.54853801169591 |
| gold | 190.953216374269 |

| Value / final-floor tied-rank Spearman | Cases | Correlation |
| --- | --- | --- |
| act1_boss_value | 963 | 0.3989613507492107 |
| act2_entry_value | 855 | 0.4650287364694454 |

## Exact retained training window

The strict window is **(360098.7655653534, 367230.0037447115]**, all **55** complete updates **2363..2417**, each individually read from the retained own-job logs. No missing update is extrapolated.

| Job | Source / phase | GPUs | Updates | Rows | Actual optimizer steps | AMP skips |
| --- | --- | --- | --- | --- | --- | --- |
| 5341 | 4788961/c173, before #159 | 8 | 2363..2379 | 17 | 1088 | 0 |
| 5342 | a721398/c175, whole_run_159 | 8 | 2380..2417 | 38 | 2432 | 0 |

Both phases use the #157 build, pure PPO and current+next LR capped at **4.444444444444445e-5**. Updates2380..2417 explicitly log `midstart_randomization=whole_run_159`. Complete work is **3520 applied optimizer steps / 0 AMP skips**. Nominal and valid student decisions are both **57671680** (=55×8×2048×64); training episode faults in this window are **0**, which is not a long-term fault guarantee.

The actual optimizer/cursor reconciliation is:

- The closed100 anchor2362 Adam132378 was derived, not a direct saved-counter read. The now-read2363..2379 rows total1088; the saved2379/serial258 Adam133468 includes2 steps of the unfinished next update:132378+1088+2=133468.
- The2380 log reports64 cumulative steps including those2 input steps. They are **not** added separately. Updates2380..2417 total2432.
- Thus102 endpoint Adam is **135898 =132378+3520**, derived and successor-crossconfirmed, not a direct2417 saved-counter read.
- The later2418 update is **367360.2248567285 / 63 actual steps / 1 AMP skip**, after this exam's strict endpoint. Its natural2418/serial261 checkpoint is truly resumed by5343 with **Adam135968**, world8→7 and **retained partial7**.135898+63+7=135968. Both2418 and partial7 are allocated to the next104 window; neither is charged to102.
- The2379/258 input2 was consumed within2380, without an extra partial charge. The old2274/250 partial46 was closed in98;96/94/92/90 and earlier partials likewise remain closed.
- Periodic saves2372/2394/2408 and the2379 allocation-end save add no optimizer work.

The100 anchor and102 endpoint are now crossconfirmed through actual successor metadata; this does not turn them into direct saved counters at2362 or2417. There was no current large-rank checkpoint read.

| LR / KL field | Value |
| --- | --- |
| Current LR min / max / end | 4.444444444444445e-5 / 4.444444444444445e-5 / 4.444444444444445e-5 |
| Next LR | 4.444444444444445e-5 throughout |
| LR lower-bound touches | 0 |
| KL min | 0.006832737783856509 |
| KL max | 0.012568171656055643 |
| KL end | 0.009484770061578255 |

## Final five expert-round fields

Updates2413..2417 each log beta0, samples0, collectionfalse. **N/A means absent measurement while CE/expert collection is paused, not measured zero.** The70h field scope is retained.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocation | Measured bytes/lane | Memory-limit event | OOM-recovered event |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2413 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2414 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2415 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2416 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2417 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurs in these five intervals. Allocator probe warnings and terminal job failures are separate from expert-recovery events.

| Update | v2 override labels | CE grad norm | PPO grad norm for CE merge | CE scale | Combat entropy |
| --- | --- | --- | --- | --- | --- |
| 2413 | N/A | N/A | N/A | N/A | 0.3279848516302549 |
| 2414 | N/A | N/A | N/A | N/A | 0.3298628422111265 |
| 2415 | N/A | N/A | N/A | N/A | 0.328332529184161 |
| 2416 | N/A | N/A | N/A | N/A | 0.32920360882638755 |
| 2417 | N/A | N/A | N/A | N/A | 0.32909392923201053 |

Ordinary PPO/AMP/clipping continues while CE is paused:

| Update | PPO norm mean | PPO norm max | Clipped fraction | AMP scale | AMP skips | Normalized entropy |
| --- | --- | --- | --- | --- | --- | --- |
| 2413 | 0.9299729848280549 | 1.938672661781311 | 1 | 524288 | 0 | 0.2599475141164431 |
| 2414 | 0.8624938195571303 | 1.195250153541565 | 1 | 524288 | 0 | 0.26050211891120467 |
| 2415 | 0.8947217743843794 | 1.2047040462493896 | 1 | 524288 | 0 | 0.25986861573332476 |
| 2416 | 0.9860158087685704 | 1.4291207790374756 | 1 | 524288 | 0 | 0.2604321997609953 |
| 2417 | 1.029858285561204 | 1.8791333436965942 | 1 | 524288 | 0 | 0.25893539438501645 |

These are logged PPO norm statistics, not CE-merge measurements. Minibatch means of gradient norms and CE scales must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
| --- | --- | --- |
| 2413 | 8 | `2662236160, 2536816640, 2383147008, 2504093696, 2360127488, 2359402496, 2380603392, 2407047168` |
| 2414 | 8 | `2662236160, 2536816640, 2383147008, 2504093696, 2360127488, 2359402496, 2380603392, 2432737280` |
| 2415 | 8 | `2662236160, 2536816640, 2383147008, 2504093696, 2360127488, 2359402496, 2380603392, 2455674880` |
| 2416 | 8 | `2662236160, 2536816640, 2383147008, 2504093696, 2360127488, 2359402496, 2380603392, 2484117504` |
| 2417 | 8 | `2662236160, 2536816640, 2383147008, 2504093696, 2360127488, 2359402496, 2380603392, 2501550080` |

Rank7's peak increases through these observations; the other seven ranks' logged peaks remain unchanged. This short window proves neither long-term growth nor its absence. The2417 CUDA allocated/reserved peaks are5015286272/10173284352 bytes, from the retained training log, not a fresh resource query or a long-term OOM guarantee.

| Decision type at2417 | Attempted decision count | Normalized entropy |
| --- | --- | --- |
| combat | 791549 | 0.32909392923201053 |
| card_reward | 26635 | 0.1225732844340459 |
| map | 50256 | 0.051439759344275776 |
| shop | 15882 | 0.11583032206320706 |
| campfire | 24335 | 0.04495809512143554 |
| other | 139919 | 0.024605264562329296 |

## Audit execution and provenance

The independent CPU audit ran **once**, **3.8781309127807617 seconds**, exit0, empty stderr. Strictfinite12435074-parameter loading,16 result/ledger shards, consolidated ledger, all paired comparisons, all55 actual training rows, cursor/Adam reconciliation and the final five expert rounds passed. There was one strict weight load.

No GPU evaluation/test, seed rerun, engine or training change, new marker, collector clear, forced save, worker signal, extra resource query or current large-rank read was performed. Closed100 model/ledger/window audits were not repeated; its retained result shards are read only as the paired baseline.

| Original retained locally and remotely under c180-preparation | Bytes | SHA-256 |
| --- | --- | --- |
| audit102.py | 16196 | `8cf1480b2916e49549452575241b8de6e8ea123fb03906fba43909186886a093` |
| audit-process.original.json | 645 | `eb3ac423ef27370b077fe01e9989565a6236ebb3eedef77e0d3bc1b2398f12f4` |
| evaluation102.audit.original.json | 200858 | `fb2a4eb5ab605847ce388f1177690aa957dee0560b5b822493d54ee8bc8748cd` |

The closed100 anchor receipt is6962 bytes, SHA-256 `79ccf56007cfc45f59d0b39489ebb807c0138230e9023bd89153fc17d1e42638`.

Ordinary102 is CLOSED after audit and publication. Next ordinary104h **374400** has strict LR window **>367230.0037447115**. The simultaneous8h supplement at104h remains **argmax/combat only**, per #158. The post-exam2418/serial261 partial7 must be accounted once in that next window; input2379/258 partial2 and all older closed partials must not be added again. The #160/#163 model switch remains Claude's decision.

## Retained shard and own-log identities

| Artifact | Bytes | SHA-256 |
| --- | --- | --- |
| evaluation-2417-rank-0.json.gz | 1368 | `ecd916925ac5cf3ec0b876a482a8cd709cf034bb39499b53c185f6638a01657e` |
| evaluation-2417-ledger-rank-0.json.gz | 14616 | `24a99c161b6565d933f2711be6232c029dcefd82cb604ef200488b5b9fb13850` |
| evaluation-2417-rank-1.json.gz | 1339 | `797404432eef971d436946ad6f3f19ee8e1fb2c9c0f20a929777f97e3ba48a78` |
| evaluation-2417-ledger-rank-1.json.gz | 14549 | `abacc3c1acd08d0598c53cf6ce51f723083af360b4bbef3622f0ca89023ff593` |
| evaluation-2417-rank-2.json.gz | 1342 | `9ce83a994c00e9e41c24b1ea2743a93e47fa2fd70ef20f7dcefbf8965b04210f` |
| evaluation-2417-ledger-rank-2.json.gz | 14437 | `13d9c92e3a2788419599ab6416d539c39a6ea2cd9eeb42eea0470f597f7853b2` |
| evaluation-2417-rank-3.json.gz | 1316 | `2a1c5310b0b68e5ddb9f56bfcb08f56ab52f26a231eb3ab0f9a0b12d92277de5` |
| evaluation-2417-ledger-rank-3.json.gz | 14535 | `e696197111a754bff5689b25ccccd9895c976cddd5541c68b059dd85843b3fc5` |
| evaluation-2417-rank-4.json.gz | 1310 | `2b262a1215eed210b52968c0d4f43d57535ae9d1e77c7471f92c5a1c934dfce3` |
| evaluation-2417-ledger-rank-4.json.gz | 14813 | `6c3a3f5e86671ecc9e31ef69f7fe8c99e7c9b0940ab7bd42d69d16dc6cfa8e42` |
| evaluation-2417-rank-5.json.gz | 1244 | `b5ddb0968a477a53e2db16195a46af8a34f898157886169e1ec610005ddc5409` |
| evaluation-2417-ledger-rank-5.json.gz | 14379 | `1a9be87c7a449cb3e80270e1bf55d71d940e3bb625b69b4196344cb6251e9562` |
| evaluation-2417-rank-6.json.gz | 1337 | `8dfd25c15618429be5c8cfbcf21e35a95f0852e42e8dc3fcfd1a3d83f649a6d0` |
| evaluation-2417-ledger-rank-6.json.gz | 15461 | `b5234443ce18c6bb5bb834051a8caac7ec03a2feef2c769112afd07af2096bf0` |
| evaluation-2417-rank-7.json.gz | 1335 | `fb0a2d76d9168cf5c720e4aeced853af5db8a788397b2146076cb7e6aa2b76dd` |
| evaluation-2417-ledger-rank-7.json.gz | 14178 | `d6b66b657a3ebc9195fcd03f7f1291eb272b8a245838b435c451949a7d7c9441` |

| Job | Own log | Bytes | SHA-256 |
| --- | --- | --- | --- |
| 5341 | job-5341.log | 414981 | `e5f2b114d1734f74af1cef09a074d62114b974f89b773ef214d294b270ed5b34` |
| 5342 | job-5342.log | 171248 | `15a566c178a7adb2d4ee08c6c0389866db30ca3a20bbe787e5424c0584054d6a` |

These hashes identify the files at audit read time; they do not promise immutable live log files.
