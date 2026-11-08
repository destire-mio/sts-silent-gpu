# E4 126-hour evaluation

The regular 1,024-seed argmax evaluation completed on four V100s at student update **24**, training clock **453758.8867663808 s**. Job 5391 ran the 768×12 pre-LN student in distillation with runtime `bcf110b4356296e5037d90e97acb50e153587aef` and the #157 engine build. Mean floor was **16.982421875**, below the 30.96 gate for switching to PPO. Distillation continues and the 8-hour supplemental evaluation remains paused.

| Outcome | Count |
| --- | ---: |
| Act 1 boss / Act 2 boss / Act 3 first boss | 312 / 18 / 2 |
| Act 3 second-boss flag / Heart | 0 / 0 |
| Ruby / emerald / sapphire | 1 / 83 / 5 |
| All three keys / Act 4 entries / Shield and Spear wins | 0 / 0 / 0 |
| Ledger death / act3_win / stalled | 1,023 / 0 / 1 |

| Paired argmax reference | Mean floor difference | Paired SE | Seeds |
| --- | ---: | ---: | ---: |
| dev-124 | 6.771484375 | 0.2579912604211543 | 1024 |
| dev-112 teacher | -14.9794921875 | 0.42363044785067644 | 1024 |
| dev-70 | -12.3671875 | 0.43071156183844606 | 1024 |

The paired seed range was 2000000000–2000001023. The audit matched the stored baseline floor vector against the dev-112 teacher and found zero contradictions in boss or key progression.

## Training interval

The strict accounting window was **(446590.3835272401, 453758.8867663808]** training seconds. Job 5391 completed distillation updates 12–24 with 416 applied optimizer steps, zero AMP skips, 6,815,744 nominal and valid actions, and zero faults. The learner used LR 1.5e-4, microbatch 256 and full encoder recomputation. Adam 256 and zero retained partial steps were observed at resume from update 8, serial 294. Adam 352 at the start of this window and 768 at its end are inferred from applied steps. The natural model snapshot contains model weights only.

All five diagnostic rows used observation version 14, with expert beta and samples at zero and expert collection disabled. The interval contained zero expert memory-limit or OOM-recovery events.

| Update | Distillation KL | Value MSE | Learner / round seconds | Peak allocated / reserved bytes | Maximum entities / candidates |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 20 | 0.41439118546715636 | 0.0008975081716864951 | 414.513 / 568.078 | 8219618304 / 11299454976 | 260 / 43 |
| 21 | 0.39478101377295893 | 0.00082851173044142 | 414.350 / 544.045 | 8372218880 / 11509170176 | 268 / 53 |
| 22 | 0.40438761020240444 | 0.0007416257390753194 | 420.194 / 550.657 | 7008929792 / 11509170176 | 193 / 49 |
| 23 | 0.38504485203774286 | 0.0007759480075596947 | 417.063 / 555.547 | 7190203392 / 11509170176 | 206 / 48 |
| 24 | 0.4005470949915205 | 0.0006160239967635603 | 421.097 / 558.354 | 7304080384 / 11509170176 | 212 / 52 |

| Update | Gradient norm mean / max | AMP scale | Combat entropy | Per-rank RSS peaks (bytes) |
| ---: | ---: | ---: | ---: | --- |
| 20 | 6.122112557291985 / 15.192207336425781 | 131072 | 0.2994543827488094 | 3669061632 / 2645065728 / 2636468224 / 2637824000 |
| 21 | 5.811363555490971 / 11.287245750427246 | 131072 | 0.3007304998975863 | 3673448448 / 2645065728 / 2636468224 / 2638741504 |
| 22 | 4.11246114782989 / 12.841009140014648 | 131072 | 0.2991558749664211 | 3673448448 / 2645065728 / 2636468224 / 2638741504 |
| 23 | 5.075010184198618 / 12.171473503112793 | 131072 | 0.2994502695818133 | 3673448448 / 2665250816 / 2636468224 / 2638741504 |
| 24 | 3.660231029614806 / 8.105717658996582 | 131072 | 0.3000966933124454 | 3673448448 / 2665250816 / 2640138240 / 2638741504 |

## Independent CPU audit

One CPU process completed in 8.57366681098938 seconds with exit 0 and empty stderr. One strict load verified 90,402,050 finite model parameters. The audit reconciled existing evaluation shards, ledger shards, paired references and all 13 training rows.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-126.pt` | 361658543 | `22bcde61e07c4a5ad4409bab94081b97610515a243d72c009a7e130f34799ae2` |
| `evaluation-24-act2-ledger.json` | 1017397 | `220dd4834920573ee13ef58aecc0b9d2bb726942d4bb32ee4b8c4350e92f31e6` |
| `evaluation126.audit.original.json` | 53789 | `747662d1add83ad518e618e0f767a50cda391f86f2fd84b2bb5e9bc763961533` |
| `audit126.process.json` | 447 | `aed6a5ca556c500c1a432c0519ee7cb4fb8bb9e7550c3f0fc56e47f7d82d654a` |
