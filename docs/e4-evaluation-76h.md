# E4 evaluation at 76 training hours

**From dev-70 rollback; pure PPO; 7 GPUs.** The ordinary evaluation completed at update **1654**, after **273692.74861606734** accumulated training seconds, with observation version **14**. Argmax was used in **1,024** games from the start, with fixed seeds 2000000000–2000001023 assigned as `seeds[rank::7]` across seven physical GPU ranks. Independent merging verified all seven result shards and seven ledger shards, with every seed present exactly once.

| Metric | 70h | 74h | 76h |
| --- | ---: | ---: | ---: |
| Mean floor | 29.349609 | 30.378906 | 29.809570 |
| Act 1 boss passed | 810 | 847 | 826 |
| Act 2 boss passed | 205 | 218 | 218 |
| First Act 3 boss passed | 44 | 55 | 54 |
| Act 3 wins | 11 | 14 | 13 |
| Heart wins | 0 | 0 | 0 |

The paired mean floor difference from 74h was **−0.569336** (SE **0.379020**). Relative to the retained dev-70 checkpoint used for the rollback, it was **+0.459961** (SE **0.401172**). Relative to the fixed teacher baseline, it was **+12.339844** (SE **0.379604**). Each comparison uses all 1,024 matched seeds.

The mean is below 74h on this fixed seed set. This comparison does not establish a change on other seeds or isolate a causal effect of pausing expert CE. The dev-70 weight rollback, retained optimizer state, elapsed training and changing GPU counts remain part of this training history.

There were **1009 deaths**, **13 Act 3 wins** and **2 stalled games**. Ruby, emerald and sapphire key counts were **24, 150 and 2**. No game collected all three keys, entered Act 4, defeated Shield and Spear, or defeated the Heart. Independent key/Act 4 checks found **0 contradictions**.

## Persisted training window and recovery

The window `(266513.3190480794, 273692.74861606734]` contains **57 complete PPO updates**. All were on the pure-PPO branch after the one-time dev-70 weight rollback at update 1561. Adam, value calibration, accumulated clock, pools and RNG continued; the persisted rollback marker prevented another weight reset.

| Allocation | Included updates | Mode | GPUs | Complete updates | Logged optimizer steps |
| --- | --- | --- | ---: | ---: | ---: |
| 4933 | 1598–1616 | Pure PPO | 8 | 19 | 1216 |
| 4934 | 1617–1626 | Pure PPO | 8 | 10 | 640 |
| 4935 | 1627–1654 | Pure PPO | 7 | 28 | 1568 |

Allocation 4933 ended with Slurm `OUT_OF_MEMORY`. Its last durable checkpoint was **1616**; its later logged updates **1617–1631** were not persisted and are excluded from the trajectory above. Allocation 4934 naturally resumed 1616 and recomputed the following updates. It then saved 1626 and yielded one GPU through the existing controller; allocation 4935 resumed on seven GPUs. Update numbers alone cannot distinguish the failed attempt from the resumed trajectory.

The complete records contain **3424 optimizer steps** and **0 AMP skips**. A further **49 already-applied partial optimizer steps** were retained at the 8-to-7 transition. The reconstructed interval therefore contains **3473 applied optimizer steps**. These 49 steps are counted once. The earlier retained partial steps 4 and 25 belong to the previous reporting window and are not added again here.

The learning rate stayed at `0.000044444444444444447`, with **0** touches of the `1e-5` lower bound. KL ranged from **0.005317180372054344** to **0.008732052427884607** and ended at **0.006724992358161183**. All included complete updates reported finite parameters.

| Allocation / GPUs | LR minimum | LR maximum | KL minimum | KL maximum | Last KL |
| --- | ---: | ---: | ---: | ---: | ---: |
| 4933 / 8 | 0.000044444444444444447 | 0.000044444444444444447 | 0.006582118 | 0.008732052 | 0.007283253 |
| 4934 / 8 | 0.000044444444444444447 | 0.000044444444444444447 | 0.006205664 | 0.007165874 | 0.006920445 |
| 4935 / 7 | 0.000044444444444444447 | 0.000044444444444444447 | 0.005317180 | 0.007709446 | 0.006724992 |

## Student counts, ledger and model verification

The final update contained **917,504 student decisions** (`7 × 2048 × 64`): combat **689329**, card reward **23424**, map **44874**, shop **13158**, campfire **22366**, and other **124353**. Expert samples are excluded. It completed **1084 fresh episodes** and **1293 midstart episodes**; midstart mean floor was **36.522042**. Training completion counts and their midstart mean are separate from the 1,024 full-run evaluation games.

The Act 2 ledger matched the merged seven ledger shards and contained **826 Act 2 entries**. Independently recomputed tied-rank Spearman correlations between value and final floor were **0.531742** at the Act 1 boss (946 cases) and **0.472210** at Act 2 entry (826 cases). These are correlations conditional on reaching the recorded subsets.

Ledger totals were **2943 card skips, 2598 card removals, 2997 rests and 2343 upgrades**. Encounter/elite fields retain their existing meaning as encountered formations, not individual kills. Death records, entry inventories, outcomes, keys and final floors matched the per-game results.

The retained weights-only `dev-76.pt` loaded strictly on CPU with **12,435,074 parameters** and finite weights. SHA-256: `4fb80b6d755adcc368a06124526db7e07a13a1f51e33d553344dbb0ef3e06acb` (49762853 bytes). Per-episode ledger SHA-256: `d883775df4b45f36d714bff898f22630bc656d5ea63c1230f69c1d3e9dff1ef2` (1613903 bytes). No model binary is published.

## Last five expert-round fields

The last five training rounds were **1650–1654**, all pure PPO on seven GPUs. In each, beta was **0**, expert samples were **0**, and expert label collection was **disabled**. A dash in the 70h-format fields below means not applicable or not computed while expert iteration is paused; it is not a measured zero.

Both `expert_lane_memory_limit` and `expert_oom_recovered` events were checked between successive worker-log update records. Neither event occurred in these five intervals. The earlier allocation-level Slurm OOM is described separately above.

| Update | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 1650 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |
| 1651 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |
| 1652 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |
| 1653 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |
| 1654 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |

No supplemental 128-seed comparison is due at 76h. The completed 72h comparison remains in the [72h report](e4-evaluation-72h.md); the next 8h comparison boundary is **80h**. Next ordinary evaluation: **78h = 280800 seconds**, with the learning-rate window strictly greater than **273692.74861606734**. No evaluation games or completed seeds were rerun for this audit.
