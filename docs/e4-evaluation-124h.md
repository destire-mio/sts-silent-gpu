# E4 124-hour evaluation

The regular 1,024-seed argmax evaluation completed on four V100s at student update **11**, training clock **446590.3835272401 s**. Job 5391 used a 768×12 pre-LN student in distillation, runtime `bcf110b4356296e5037d90e97acb50e153587aef` and the #157 engine build. The mean floor was **10.2109375**, below the 30.96 gate for switching to PPO.

| Outcome | Count |
| --- | ---: |
| Act 1 boss / Act 2 boss / Act 3 first boss | 20 / 0 / 0 |
| Act 3 second-boss flag / Heart | 0 / 0 |
| Ruby / emerald / sapphire | 0 / 22 / 0 |
| All three keys / Act 4 entries / Shield and Spear wins | 0 / 0 / 0 |
| Ledger death / act3_win / stalled | 1024 / 0 / 0 |

| Paired argmax reference | Mean floor difference | Paired SE | Seeds |
| --- | ---: | ---: | ---: |
| dev-122 | -20.26171875 | 0.37251982476049234 | 1024 |
| dev-112 teacher | -21.7509765625 | 0.37541406939482863 | 1024 |
| dev-70 | -19.138671875 | 0.376428309704651 | 1024 |

All comparisons use seeds 2000000000–2000001023. The audit independently matched the stored dev-112 baseline floor vector and found zero contradictions in boss or key progression.

## Training interval

The accounting window is **(439244.7129755752, 446590.3835272401]** training seconds. Both jobs used four V100s and the #157 engine build.

| Job / runtime | Phase | Updates | Optimizer steps | AMP skips | Nominal / valid actions | Faults |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| 5390 / `364e8e4` | 384 PPO | 2961–3008 | 1,535 | 1 | 25,165,824 / 25,165,824 | 0 |
| 5391 / `bcf110b` | 768 distillation | 9–11 | 96 | 0 | 1,572,864 / 1,572,863 | 1 |

The 384 PPO segment used LR 4.444444444444445e-5. Its Adam count at the handoff is inferred as 163535 from the inferred dev-122 count of 162000 and 1,535 applied steps. The student independently resumed checkpoint 8, serial 294, at observed Adam 256 with a zero partial cursor. Its Adam count at update 11 is inferred as 352. It used LR 1.5e-4, microbatch 256 and full encoder recomputation.

The fault after student update 9 affected the action count for update 10. The 2,424-byte record for seed 1938078572, slot 1652, `engine_fault:852769260`, matched the previously captured 5387 record byte for byte (SHA-256 `484bea63a220601c64e50a4a55c289e7f109261f865218c94849da077323d468`). The engine cause remains unverified. The current window includes the one fault observed on job 5391.

## Learner and expert diagnostics

All five rows used observation version 14 with expert beta and samples at zero and expert collection disabled. There were no expert memory-limit or OOM-recovery events in these rows. Expert-only loss and scale metrics are N/A.

| Job / update | Phase | Gradient norm mean / max | AMP scale | Combat entropy | Per-rank RSS peaks (bytes) |
| --- | --- | --- | ---: | ---: | --- |
| 5390 / 3007 | PPO | 1.3989677727222443 / 2.9210753440856934 | 262144 | 0.2732711738273729 | 2509004800 / 2346532864 / 2387058688 / 2336591872 |
| 5390 / 3008 | PPO | 1.7672167792916298 / 3.4195938110351562 | 262144 | 0.27022538871518487 | 2509004800 / 2346532864 / 2387058688 / 2336591872 |
| 5391 / 9 | distillation | 8.89927463606 / 21.428302764892578 | 131072 | 0.30098531544711105 | 2665410560 / 2627506176 / 2622066688 / 2625896448 |
| 5391 / 10 | distillation | 7.5145319774746895 / 11.534026145935059 | 131072 | 0.29924782205229944 | 2665410560 / 2628423680 / 2622984192 / 2626813952 |
| 5391 / 11 | distillation | 7.52892080321908 / 19.04714012145996 | 131072 | 0.3008586111967895 | 2665410560 / 2628423680 / 2622984192 / 2626813952 |

| Student update | Distillation KL | Value MSE | Maximum entities / candidates | Peak allocated / reserved (bytes) | Learner / round seconds |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 9 | 0.5367262295372753 | 0.0017350433294016554 | 243 / 51 | 7525343232 / 9871294464 | 415.375 / 561.278 |
| 10 | 0.5135575104409043 | 0.0013859114076261465 | 245 / 47 | 7934139392 / 11257511936 | 416.389 / 549.768 |
| 11 | 0.5117764814778193 | 0.0013600017417218146 | 229 / 51 | 7636665856 / 11257511936 | 416.798 / 562.277 |

## Independent CPU audit

One CPU process completed in 8.222136735916138 seconds with exit 0 and empty stderr. One strict load verified 90,402,050 finite model parameters. The audit reconciled the evaluation and ledger shards, paired references and 51 training rows using the naturally produced artifacts.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-124.pt` | 361658543 | `010f6673e22d05172bfeabdb8c0b37716da05da86943c249496d71e8a7dcd913` |
| `evaluation-11-act2-ledger.json` | 658597 | `90d4c1fe147293e230e9453e22e52aa3f216de5c5d8ae78dd83510f0f0cd6f67` |
| `evaluation124.audit.original.json` | 175030 | `78f489233526304472f863f2b5d6e53b80ae4d18b2be427aebde4d109c196c11` |
