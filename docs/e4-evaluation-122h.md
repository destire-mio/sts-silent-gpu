# E4 122-hour evaluation

Job 5390 completed the regular 1,024-seed argmax evaluation at PPO update **2960**, training clock **439244.7129755752 s**. It used the 384×6 post-LN policy on the dev-112 rollback lineage, four V100s, runtime `364e8e4be7ee8f8e04fa334f027e309018f7e97d` and the #157 engine build. Seeds 2000000000–2000001023 ran without search.

| Outcome | Result |
| --- | ---: |
| Mean floor | 30.47265625 |
| Act 1 boss | 848 / 1024 |
| Act 2 boss | 236 / 1024 |
| Act 3 first boss | 60 / 1024 |
| Act 3 second-boss flag | 11 / 1024 |
| Heart | 0 / 1024 |
| Ruby / emerald / sapphire | 69 / 174 / 58 |
| All three keys / Act 4 entries / Shield and Spear wins | 27 / 1 / 0 |
| Ledger death / act3_win / stalled | 1011 / 11 / 2 |

| Paired argmax reference | Mean floor difference | Paired SE | Seeds |
| --- | ---: | ---: | ---: |
| dev-120 | 0.6455078125 | 0.4119659995988924 | 1024 |
| dev-112 | -1.4892578125 | 0.4113486556449253 | 1024 |
| dev-70 | 1.123046875 | 0.42717589249006666 | 1024 |

The stored baseline label is `dev-16.pt argmax`. Its floor vector matched the independently read dev-112 result on every seed. All 1,024 episodes enter each comparison. The audit found zero boss or key progression contradictions. The ledger label `act3_win` records the Act 3 second-boss flag.

## Training interval

The strict window is **(432097.7652507751, 439244.7129755752]** training seconds. All 61 completed updates (2900–2960) came from job 5390 in the 384×6 PPO phase on four V100s. They recorded 1,952 applied optimizer steps, zero AMP skips, and 31,981,568 nominal and valid actions, with zero episode faults. The preceding 768 retry on job 5389 ended during the 120-hour supplement before any new distillation update.

Job 5390 restored update 2899, serial 303, Adam 160048, and a zero partial cursor with all four ranks matching. The endpoint Adam count of 162000 is inferred from the 1,952 applied steps. Current and next LR held at 4.444444444444445e-5, with zero lower-bound touches. KL min / max / end was 0.006744666387282644 / 0.01696791309479981 / 0.00990360374323464.

## Last five PPO updates

Expert beta and samples were zero, with collection disabled. Expert lane memory-limit and OOM-recovery events were absent in these five updates. Expert-only loss, scale, selection and related metrics are N/A.

| Update | PPO gradient norm mean / max | AMP scale | Combat entropy | RSS peaks per rank (bytes) |
| ---: | --- | ---: | ---: | --- |
| 2956 | 1.0126731768250465 / 1.551397681236267 | 262144 | 0.2798553468965644 | 2509004800 / 2342277120 / 2380886016 / 2334339072 |
| 2957 | 1.46731242723763 / 4.583284378051758 | 262144 | 0.28341139795010756 | 2509004800 / 2342277120 / 2380886016 / 2334339072 |
| 2958 | 1.3964108023792505 / 2.7245609760284424 | 262144 | 0.28271473051368234 | 2509004800 / 2342277120 / 2380886016 / 2334339072 |
| 2959 | 1.520123640075326 / 2.3802554607391357 | 262144 | 0.2857766886765735 | 2509004800 / 2342277120 / 2380886016 / 2334339072 |
| 2960 | 1.3155587315559387 / 2.9285888671875 | 262144 | 0.28425910873582483 | 2509004800 / 2342277120 / 2380886016 / 2334339072 |

## Independent CPU audit

The first CPU process exited in 3.0669777393341064 seconds on a stale 120-hour fault-count assertion before model loading. The corrected process completed in 2.88588809967041 seconds with exit 0 and empty stderr. One strict model load verified 12,435,074 finite parameters. The audit reconciled four result shards, four ledger shards, the combined ledger, paired references and all 61 training rows. It reused the natural evaluation outputs.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-122.pt` | 49768265 | `4b628a65fa3d71aac371429b9678527073bfe7ca8c47ba953c498c44a904ecf8` |
| `evaluation-2960-act2-ledger.json` | 1653181 | `50028fc7fb1b633cd0e519bc5b4a5548f65095a0a5d9b4b45ac7506fc502d23c` |
| `evaluation122.audit.original.json` | 203237 | `edae29b2f2434ef875a5dea8f1028f6df506de794c76e89f2a58b4b31bb1bdaa` |
