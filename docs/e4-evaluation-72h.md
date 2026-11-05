# E4 evaluation at 72 training hours

The ordinary evaluation completed at update **1539**, after **259242.08943709722** accumulated training seconds, using observation version **14**. The policy selected actions by argmax in **1,024** games from the start, with fixed seeds 2000000000–2000001023. The three physical GPU ranks owned seeds `rank::3`; the counts below were independently recomputed from all three result shards and all three ledger shards.

| Metric | 70h | 72h |
| --- | ---: | ---: |
| Mean floor | 29.349609 | 26.747070 |
| Act 1 boss passed | 810 | 754 |
| Act 2 boss passed | 205 | 119 |
| First Act 3 boss passed | 44 | 20 |
| Act 3 wins | 11 | 2 |
| Heart wins | 0 | 0 |

The paired mean floor difference from 70h was **-2.602539** (SE 0.357093). The paired difference from the fixed teacher baseline was **+9.277344** (SE 0.360160). Each comparison uses all 1,024 matched seeds. Performance declined on this fixed seed set. This window includes both a change in the expert memory estimate and an 8-to-3 GPU transition with a smaller global rollout; the comparison does not isolate the cause of the decline.

There were **1022 deaths**, **2 Act 3 wins** and **0 stalled games**. Ruby, emerald and sapphire key counts were **5, 134 and 4**. No game collected all three keys, entered Act 4, defeated Shield and Spear, or defeated the Heart. Independent key/Act 4 consistency checks found no contradiction.

## Training window and verification

The interval `(252078.49940035964, 259242.08943709722]` contains **47 PPO updates**:

| Allocation | Included updates | GPU count |
| --- | --- | ---: |
| 4597 | 1493–1514 | 8 |
| 4598 | 1515–1525 | 8 |
| 4731 | 1526–1531 | 8 |
| 4843 | 1532–1533 | 3 |
| 4929 | 1534–1539 | 3 |

Only this persisted chain was used. Older orphan updates and the workerless allocations 4622/4672 were excluded. The 8-to-3 transition retained 33 optimizer steps already applied in an unfinished learner round, then recollected the incompatible unfinished cursor. The 47 complete update records contain **2688 optimizer steps** and **0 AMP skips**; the retained 33 steps are additional applied work, not a second charge or a subtraction from the new 3-GPU update records.

Learning rate was `0.000013168724279835392` throughout, with no touch of the `1e-5` lower bound. KL ranged from 0.005006986 to 0.016538814 and ended at 0.005048261. All included update records reported finite parameters.

The last update contained **393,216** student decisions: combat 294063, card reward 10205, map 19672, shop 6003, campfire 9430, and other 53843. This is `3 × 2048 × 64`; the earlier 8-GPU rounds contained 1,048,576 decisions. Expert samples are excluded from these PPO counts. Last-update fresh episodes completed: 510; midstart episodes completed: 668, mean floor 34.582335.

The per-episode Act 2 ledger matched the merged rank shards and contained **754 Act 2 entries**. Tied-rank Spearman correlations between value and final floor were **0.486645** at the Act 1 boss (921 cases) and **0.455134** at Act 2 entry (754 cases). Each correlation is conditional on its recorded subset. Ledger totals were 2157 card skips, 2048 card removals, 3075 rests and 1796 upgrades. Encounter/elite fields retain their existing meaning as encountered formations, not individual kills.

The retained weights-only `dev-72.pt` loaded strictly on CPU with **12,435,074 parameters** and finite weights. Its SHA-256 is `09c2a451c1308874c09d7ef5a9755657821e1e1dd8918d5b8e4d04126af6c4e6` (49762853 bytes). The per-episode ledger SHA-256 is `cfdbe9d2d1d4bca2c06aea639388c49736dd0115e9b36021caaa8be86f3057a5` (1553575 bytes). No model binary is published.

## Last five expert rounds

These are the last five rounds in this training window, all from allocation 4929 on three GPUs. Fields follow the 70h table. Samples and throughput are the logged global expert metrics; memory columns are bytes. Events are grouped by their position between successive update records in the existing worker log. Both `expert_lane_memory_limit` and `expert_oom_recovered` were checked. A dash indicates no matching event in that interval.

| Update | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 1535 | 62 | 31 | 266 | 12.56 | 4 | 28.32% | 0.8600 | 73.68% | 2845239808 | 6438642418 | 10936362 | — | — |
| 1536 | 31 | 31 | 317 | 14.78 | 8 | 19.83% | 0.6561 | 79.50% | 2529091584 | 5710273145 | 15644293 | — | — |
| 1537 | 31 | 31 | 353 | 15.46 | 10 | 20.96% | 0.7907 | 78.47% | 2525965824 | 5635731256 | 19080226 | — | — |
| 1538 | 31 | 31 | 363 | 15.82 | 9 | 21.43% | 0.8493 | 79.06% | 2526078976 | 5795502125 | 14961351 | — | — |
| 1539 | 31 | 31 | 326 | 14.46 | 9 | 21.18% | 0.6171 | 80.37% | 3017544192 | 5870841901 | 17138903 | — | — |

No memory-limit or expert OOM recovery event was recorded in these five rounds. Update 1535 used 62 lanes and requested 31 for the next round; updates 1536–1539 used 31 lanes. These measurements do not establish maximum safe capacity or attribute the evaluation change to expert iteration.

## Supplemental first-128-seed comparison

The natural 72h hook is running independent argmax and combat lookahead evaluations of the same `dev-72.pt` on seeds 2000000000–2000000127. Eight logical receipt shards are assigned across the three physical GPUs. Completed seeds are not rerun, and serial shards share the one-hour combat cap. The paired comparison will use the completed-seed intersection. At publication, the argmax mode has completed and combat is still in progress; no final combat score, intersection result, or exact combat wall time is claimed here.
