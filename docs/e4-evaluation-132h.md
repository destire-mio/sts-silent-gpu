# E4 132-hour ordinary evaluation

Job 5431 used two V100s and runtime `4ce90335d51d63f9cfe2ece565e6743c7cebd16e`. The naturally saved 768×12 pre-LN student at update 62 and training clock 475739.6731542035 s completed all 1,024 argmax cases with no save/load during play. Its mean floor was **31.328125**, above the 30.96 gate. The same job emitted `distillation_complete` at update 62 with the PPO learning rate set to 4.444444444444445e-5.

| Metric | Count |
| --- | ---: |
| Act 1 / Act 2 / first Act 3 boss | 838 / 275 / 67 |
| Second Act 3 boss / Act 4 entry | 17 / 4 |
| Shield and Spear / Heart | 2 / 0 |
| Ruby / emerald / sapphire / all three | 87 / 237 / 84 / 23 |
| Death / Act 3 win / stalled | 1,006 / 17 / 1 |
| Progression contradictions | 0 |

| Paired comparison over 1,024 seeds | Mean floor change | Paired SE |
| --- | ---: | ---: |
| dev-130 | 3.453125 | 0.43919835788100514 |
| dev-112 | -0.6337890625 | 0.449909187493391 |
| dev-70 | 1.978515625 | 0.4485128615177643 |

The saved baseline and independent dev-112 evaluation had identical per-seed floors. The Act 2 ledger recorded 838 entries. Tied-rank Spearman with final floor was 0.424964 for Act 1 boss value (937 cases) and 0.445308 for Act 2 entry value (838 cases).

## Training window (468059.30243818165, 475739.6731542035]

| Job | Complete updates in its log | Logged attempts / applied | Applied on this job | Valid decisions |
| --- | --- | ---: | ---: | ---: |
| 5430 | 50–54 | 640 / 640 | 722 | 1,310,720 |
| 5431 | 55–62 | 1,024 / 1,024 | 942 | 2,097,152 |

The logs showed zero AMP skips and zero episode faults in the window. The 82 already-applied steps of update 55 belong to 5430 and are included in 5431's completed-update log. Adam was observed at 3945 on resume, with 82 active partial steps at serial 309. The start (3223) and end (4887) are inferred from the audited step counts.

## Independent CPU audit

One CPU process completed in 8.549317210912704 s with exit 0 and empty stderr. It reconciled all natural case and ledger shards, checked progression and paired metrics, and strictly loaded 90,402,050 finite parameters once.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| dev-132.pt | 361658543 | `d889dae3291ea0fdcf3ec091bbb3eb678929f4b47ac5cc2177816d7d5cdd4019` |
| evaluation-62-act2-ledger.json | 1635468 | `22331cd636fe22545dea1e937a191cc49f851ad87d49b8e22d7570f4aea807ba` |
| evaluation132.audit.original.json | 19514 | `2b6f0d9b0be95df98ceac1ed8c9e6186573d5659781af4a3c5e39cf46823702e` |
| audit132.process.json | 116 | `a4623bf5ee71195c22e23953d42ec342fb7f2ebd3bd1bdf1e6403c5ed3af4b39` |
