# E4 116-hour evaluation

The 768×12 pre-LN student completed the regular 1,024-seed argmax evaluation at distillation update **7 of 40**, at training clock **417746.18399990676 s**. Job 5387 used four V100s, runtime `30eb3357d32abbd27b9decbf157c5ddd80d54075`, and the #157 engine build. The evaluation used seeds 2000000000–2000001023 and ran without search.

| Outcome | Result |
| --- | ---: |
| Mean floor | 8.564453125 |
| Act 1 boss | 12 / 1024 |
| Act 2 boss | 0 / 1024 |
| Act 3 first boss | 0 / 1024 |
| Act 3 second-boss flag | 0 / 1024 |
| Heart | 0 / 1024 |
| Ruby / emerald / sapphire | 0 / 13 / 0 |
| All three keys / Act 4 entries / Shield and Spear wins | 0 / 0 / 0 |
| Ledger death / act3_win / stalled | 1024 / 0 / 0 |

| Paired argmax reference | Mean floor difference | Paired SE | Seeds |
| --- | ---: | ---: | ---: |
| dev-112 teacher | -23.3974609375 | 0.3704478323876195 | 1024 |
| dev-114 | -22.8017578125 | 0.37886817084878255 | 1024 |
| dev-70 | -20.78515625 | 0.37128888056731324 | 1024 |

All episodes entered the mean and all three paired comparisons. The independent audit found zero boss or key progression contradictions. This evaluation measures the student early in distillation. The student continued training after the evaluation and completed update 8 with finite metrics.

## Training interval

The strict interval is **(410439.8423708107, 417746.18399990676]** in training seconds. Both segments used four V100s and the #157 build. Student and teacher-era optimizer histories are separate.

| Job / runtime | Model and phase | Complete updates | Optimizer steps | AMP skips | Nominal / valid actions | Episode faults |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| 5383 / `a7213984` | 384×6 PPO | 2755–2783 | 927 | 1 | 15204352 / 15204352 | 0 |
| 5387 / `30eb3357` | 768×12 distillation | 1–7 | 224 | 0 | 3670016 / 3670016 | 0 |

The single AMP skip was in 5383 update 2781. The 18 and 46 retained steps reported at earlier evaluations are already accounted for. Periodic saves add no optimizer steps. The student began with a fresh optimizer at handoff. The 224 student steps are counted from complete update logs. The evaluation snapshot contains model weights only.

Current and next learning rates were `4.444444444444445e-5` throughout, with zero lower-bound touches. PPO KL min/max/end were `0.0076731016357717285 / 0.014260765642524986 / 0.012274792137343865`. Distillation KL min/max/end were `0.0025862541775234504 / 0.29130663072191965 / 0.014837888280953226`. The distillation maximum occurred on update 1.

## Activation recomputation

All seven rounds used full layer recomputation on four V100s, microbatch 512 and 524288 valid mainline actions per round. The memory gate rejected reduced recomputation on rounds 2–6. The runtime selected full on round 7. The selection reflects the gate and the trial policy. Comparative timings require measured reduced rounds.

| Update | Requested | Actual | Lowest estimated headroom (B) | Max entities / candidates | Round / learner (s) | Max allocated / reserved across ranks (B) |
| ---: | --- | --- | ---: | ---: | ---: | ---: |
| 1 | full | full | N/A | 236 / 51 | 558.833 / 424.842 | 10628237824 / 14342422528 |
| 2 | reduced | full | -42478010368 | 188 / 55 | 538.293 / 414.356 | 9327567872 / 14306770944 |
| 3 | reduced | full | -47807987712 | 209 / 51 | 553.851 / 415.962 | 10355556864 / 14086569984 |
| 4 | reduced | full | -48846077952 | 213 / 49 | 546.991 / 419.712 | 10079915008 / 14086569984 |
| 5 | reduced | full | -50694651904 | 220 / 45 | 550.751 / 415.379 | 10070758912 / 14262730752 |
| 6 | reduced | full | -52263321600 | 226 / 55 | 551.034 / 419.165 | 10600197120 / 14231273472 |
| 7 | full | full | N/A | 259 / 49 | 567.853 / 423.300 | 12109915136 / 14308868096 |

## Learner diagnostics and audit

Updates 3–7 recorded expert beta 0, expert samples 0 and expert collection false. Expert-specific norms and scale were unavailable. The window contained zero expert memory-limit and expert OOM-recovery events in those five updates. The job log contains CUDA allocator warnings in intervals followed by finite completed updates. The worker was still running after update 8.

| Update | Pre-clip gradient norm mean / max | AMP scale | Combat entropy | Peak RSS by rank (B) |
| ---: | ---: | ---: | ---: | --- |
| 3 | 7.3794712126255035 / 16.191543579101562 | 131072 | 0.30000046516629153 | 2586578944 / 2392154112 / 2426806272 / 2390298624 |
| 4 | 6.778085067868233 / 14.195202827453613 | 131072 | 0.3006254156937045 | 2586578944 / 2393989120 / 2426806272 / 2400694272 |
| 5 | 6.778334677219391 / 11.694514274597168 | 131072 | 0.30119267616700884 | 3590451200 / 2417844224 / 2451578880 / 2425466880 |
| 6 | 6.065923102200031 / 12.080863952636719 | 131072 | 0.30189092196453643 | 3590451200 / 2417844224 / 2451578880 / 2425466880 |
| 7 | 9.302091773599386 / 30.330615997314453 | 131072 | 0.30187082491207323 | 3590451200 / 2417844224 / 2451578880 / 2425466880 |

One independent CPU audit strictly loaded all 90,402,050 parameters and verified finite weights, the 12 pre-LN layers and the final LayerNorm. The audit matched four result shards and four ledger shards to the natural 1,024-episode summary and ledger. It recomputed the paired estimates and audited the 36 complete training updates. Audit process exit 0, 15.729 s, empty stderr. The audit used existing evaluation artifacts and ran zero additional seeds.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-116.pt` | 361658543 | `a7fcd964e6bcdfeec3bab825701d6976b8c5986af8f9d721cb7fb26e4917ed75` |
| `evaluation-7.json` | 1410 | `4c51230756d953492c7517fd170b2a5de43dd1ae226dd9302d8a74e1fe927266` |
| `evaluation-7-act2-ledger.json` | 652578 | `ab7e5d1d88c4255278ea9848641e3b324c859e1551889ff900be818a4c799cd2` |
| `evaluation116.audit.original.json` | 129039 | `6f27d75570711cb0b3519195e636c9c892491038e2fec8cd5f8d9e02fbe4a4e7` |
