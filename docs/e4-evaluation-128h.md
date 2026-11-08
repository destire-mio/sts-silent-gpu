# E4 128-hour evaluation

The regular 1,024-seed argmax evaluation completed on two V100s at student update **37**, training clock **461163.6338208656 s**. Job 5429 ran the 768×12 pre-LN student in distillation with runtime `4ce90335d51d63f9cfe2ece565e6743c7cebd16e`. Mean floor was **25.775390625**, below the 30.96 gate for switching to PPO. Distillation continues and the 8-hour supplemental evaluation remains paused.

| Outcome | Count |
| --- | ---: |
| Act 1 boss / Act 2 boss / Act 3 first boss | 712 / 141 / 34 |
| Act 3 second-boss flag / Heart | 10 / 0 |
| Ruby / emerald / sapphire | 33 / 162 / 57 |
| All three keys / Act 4 entries / Shield and Spear wins | 7 / 1 / 1 |
| Ledger death / act3_win / stalled | 1013 / 10 / 1 |

| Paired argmax reference | Mean floor difference | Paired SE | Seeds |
| --- | ---: | ---: | ---: |
| dev-126 | 8.79296875 | 0.41247291070329734 | 1024 |
| dev-112 teacher | -6.1865234375 | 0.4506127520842281 | 1024 |
| dev-70 | -3.57421875 | 0.45848711309164714 | 1024 |

The audit matched all seeds 2000000000–2000001023 and confirmed that the stored baseline floor vector matches dev-112. It found zero contradictions in boss or key progression.

## Training interval

The strict window was **(453758.8867663808, 461163.6338208656]** training seconds.

| Job | GPUs | Completed updates | Global minibatch | Attempts / applied / AMP skips | Valid actions | Faults |
| --- | ---: | --- | ---: | --- | ---: | ---: |
| 5391, c194 `bcf110b4356296e5037d90e97acb50e153587aef` | 4 | 25–32 | 32768 | 256 / 256 / 0 | 4194304 | 0 |
| 5429, c196 `4ce90335d51d63f9cfe2ece565e6743c7cebd16e` | 2 | 33–37 | 4096 | 640 / 640 / 0 | 1310720 | 0 |

The learner used microbatch 256, full encoder recomputation and LR 1.5e-4. Adam 768 at the window start and 1024 after complete update 32 are inferred. The natural save at update 32, serial 301, retained 23 already-applied partial steps. The controller observed Adam 1047 at the 5429 resume and reset the active cursor count to zero for the change from four to two GPUs. The 23 steps are included once in the optimizer total and are separate from the completed-round action counts. Adam 1687 at update 37 is inferred. The eval snapshot contains model weights only.

| Update | Distillation KL | Value MSE | Learner / round seconds | Peak allocated / reserved bytes | Max entities / candidates |
| ---: | ---: | ---: | ---: | --- | --- |
| 33 | 0.3525361265787552 | 0.0004388691526692876 | 444.823 / 565.043 | 6305062912 / 8048869376 | 179 / 48 |
| 34 | 0.32666933483305943 | 0.0002395164583505376 | 444.168 / 569.176 | 6942069760 / 9768534016 | 193 / 44 |
| 35 | 0.32035009727141783 | 0.0002557638634760773 | 448.623 / 581.548 | 7026092544 / 9894363136 | 197 / 51 |
| 36 | 0.31151475992221067 | 0.00019802044425780102 | 459.428 / 597.787 | 7152002048 / 10083106816 | 204 / 48 |
| 37 | 0.31017879488042105 | 0.00020087634883352192 | 467.027 / 606.293 | 7551947776 / 10670309376 | 225 / 51 |

## Independent CPU audit

One CPU process completed in 8.292162656784058 seconds with exit 0 and empty stderr. One strict load verified 90,402,050 finite model parameters. The audit reconciled existing episode and ledger shards, the paired references and both training segments.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-128.pt` | 361658543 | `288a33f941d8272e10602e0468f4b27982680fc1155a51796c97f9ddfbd02be4` |
| `evaluation-37-act2-ledger.json` | 1465430 | `1ee5eb33c3f38361bdbcec3c00516692948791a99f18846bac8a8c568600fd35` |
| `evaluation128.audit.original.json` | 19206 | `5fef1ddfa9903fe38180186ac2b080b76af119ca0c55da936492941e11fa9deb` |
| `audit128.process.json` | 291 | `8f02920e95b7f4cc139f1e0a18984cb6085bfcdd8386110962166cd67307429a` |
