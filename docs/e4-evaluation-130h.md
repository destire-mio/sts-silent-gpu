# E4 130-hour evaluation

The regular 1,024-seed argmax evaluation reached a mean floor of **27.875** at student update **49** and training clock **468059.30243818165 s**. Job 5430 used two V100s and runtime `4ce90335d51d63f9cfe2ece565e6743c7cebd16e` for the 768×12 pre-LN student. The 30.96 threshold for switching to PPO is still ahead. Distillation continues and the supplemental evaluation remains paused.

| Outcome | Count |
| --- | ---: |
| Act 1 boss / Act 2 boss / Act 3 first boss | 754 / 193 / 46 |
| Act 3 second-boss flag / Heart | 7 / 0 |
| Ruby / emerald / sapphire | 66 / 197 / 72 |
| All three keys / Act 4 entries / Shield and Spear wins | 19 / 2 / 2 |
| Ledger death / act3_win / stalled | 1015 / 7 / 2 |

| Paired argmax reference | Mean floor difference | Paired SE | Seeds |
| --- | ---: | ---: | ---: |
| dev-128 | 2.099609375 | 0.4622005382764099 | 1024 |
| dev-112 teacher | -4.0869140625 | 0.4420230176431246 | 1024 |
| dev-70 | -1.474609375 | 0.46180330376121426 | 1024 |

The audit reconciled all seeds 2000000000–2000001023 and found zero progression contradictions. The stored baseline floor vector matched dev-112 for every seed.

## Training interval

The strict window was **(461163.6338208656, 468059.30243818165]** training seconds. Both segments used two V100s, global minibatch 4096, microbatch 256, full encoder recomputation and LR 1.5e-4.

| Job | Completed updates | Completed-round attempts / applied / AMP skips | Valid actions | Faults | Steps applied in this job |
| --- | --- | ---: | ---: | ---: | ---: |
| 5429 | 38–43 | 768 / 768 / 0 | 1572864 | 0 | 799 |
| 5430 | 44–49 | 768 / 768 / 0 | 1572864 | 0 | 737 |

The natural save at update 43, serial 305, retained 31 already-applied steps from update 44. The controller observed Adam 2486 and an active cursor of 31 at the 5430 resume. The 128 steps reported for update 44 include those 31. Adam 1687 at the start, 2455 after update 43 and 3223 at the end are inferred.

## Independent CPU audit

One CPU process completed in 7.701402129023336 seconds with exit 0 and empty stderr. One strict load verified 90,402,050 finite model parameters. The audit used the natural evaluation files and training logs.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-130.pt` | 361658543 | `8519c0940ff81fbd58743f781f7184628ff75f1df2ab4ca0c9070a35222a896c` |
| `evaluation-49-act2-ledger.json` | 1538355 | `f00318af201d5785d5df9af33896f82b0b67296a89b413612005b9828d8ee603` |
| `evaluation130.audit.original.json` | 18425 | `c55417fe0ee84abf81d52c717b3b3321dbdca48d8c6307943d9d4a5ff99a05dd` |
| `audit130.process.json` | 113 | `d4038a1373abc2f5c4de8cedbdacf49b90a4a430fb7589f48439fa08f0e04db7` |
