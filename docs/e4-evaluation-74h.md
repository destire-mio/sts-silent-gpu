# E4 evaluation at 74 training hours

**From dev-70 rollback; pure PPO; 8 GPUs.** The ordinary evaluation completed at update **1597**, after **266513.3190480794** accumulated training seconds. Observation version was **14**. Actions were selected by argmax in **1,024** games from the start, using fixed seeds 2000000000–2000001023 split across eight physical GPU ranks. Counts were independently recomputed from the eight result shards and eight ledger shards.

| Metric | 70h | 72h | 74h |
| --- | ---: | ---: | ---: |
| Mean floor | 29.349609 | 26.747070 | 30.378906 |
| Act 1 boss passed | 810 | 754 | 847 |
| Act 2 boss passed | 205 | 119 | 218 |
| First Act 3 boss passed | 44 | 20 | 55 |
| Act 3 wins | 11 | 2 | 14 |
| Heart wins | 0 | 0 | 0 |

The paired mean floor difference from 72h was **+3.631836** (SE 0.392364). Relative to the retained dev-70 checkpoint used for the rollback, the paired difference was **+1.029297** (SE 0.393619). Relative to the fixed teacher baseline, it was **+12.909180** (SE 0.389326). Each comparison uses the same 1,024 matched seeds.

The evaluated model improved on this fixed seed set. Model rollback, expert CE removal, elapsed training, changing GPU count/global rollout size and the existing adaptive learning rate all changed during the window. These comparisons do not isolate the causal effect of CE removal or establish results on other seeds.

There were **1008 deaths**, **14 Act 3 wins** and **2 stalled games**. Ruby, emerald and sapphire key counts were **9, 170 and 1**. No game collected all three keys, entered Act 4, defeated Shield and Spear, or defeated the Heart. Independent key/Act 4 consistency checks found no contradiction.

## Rollback and training window

The window `(259242.08943709722, 266513.3190480794]` contains **58** complete PPO update records. The model weights were restored from `dev-70.pt` at the natural start of allocation 4930, after update **1561** at training time **262082.4986588372**. Adam, the value calibration, accumulated clock and existing midstart pools/RNG were retained. A checkpointed marker prevented a repeated rollback on later GPU-count changes. The retained `dev-72.pt` was not overwritten.

The preserved clock means this evaluation includes about **4430.82 seconds** of accumulated training after the rollback, rather than two full hours starting from the rollback. It is the next scheduled 2h clock boundary.

| Allocation | Included updates | Training mode | GPUs | Complete updates | Logged optimizer steps |
| --- | --- | --- | ---: | ---: | ---: |
| 4929 | 1540–1561 | PPO with expert CE, before rollback | 3 | 22 | 528 |
| 4930 | 1562–1565 | Pure PPO, after dev-70 rollback | 4 | 4 | 128 |
| 4931 | 1566–1569 | Pure PPO, after dev-70 rollback | 6 | 4 | 192 |
| 4932 | 1570–1572 | Pure PPO, after dev-70 rollback | 5 | 3 | 120 |
| 4933 | 1573–1597 | Pure PPO, after dev-70 rollback | 8 | 25 | 1600 |

Updates **1540–1561** used expert CE before the rollback. Updates **1562–1597** were pure PPO with `expert_beta = 0`, zero expert samples and expert collection disabled. The last model was evaluated on eight GPUs; the entire 72h-to-74h interval is not a single pure-PPO or fixed-GPU-count phase.

The complete update records contain **2568 optimizer steps** and **0 AMP skips**. Another **29 steps** had already been applied in unfinished learner rounds before GPU-count changes: 4 at the 4-to-6 transition and 25 at the 5-to-8 transition. Those Adam updates were retained; they are not subtracted again from the new-world complete-update records. The reconstructed interval therefore contains **2597 applied optimizer steps**. The pure-PPO portion contains 2040 complete-update steps plus those 29 retained partial steps. Only the persisted source ranges above were included; older orphan attempts were excluded.

The learning rate ranged from `0.000013168724279835392` to `0.000044444444444444447`, ending at `0.000044444444444444447`, with **0** touches of the `1e-5` lower bound. KL ranged from 0.002482889 to 0.019788834 and ended at 0.008662781. All included updates reported finite parameters.

| Mode / GPUs | LR minimum | LR maximum | KL minimum | KL maximum | Last KL |
| --- | ---: | ---: | ---: | ---: | ---: |
| Expert CE / 3 | 0.000013168724279835392 | 0.000013168724279835392 | 0.005236173 | 0.007888878 | 0.006298217 |
| Pure PPO / 4 | 0.000013168724279835392 | 0.00002962962962962963 | 0.002482889 | 0.006312045 | 0.006312045 |
| Pure PPO / 6 | 0.00002962962962962963 | 0.000044444444444444447 | 0.004814596 | 0.012398361 | 0.012398361 |
| Pure PPO / 5 | 0.000044444444444444447 | 0.000044444444444444447 | 0.010901876 | 0.012313507 | 0.011381940 |
| Pure PPO / 8 | 0.000044444444444444447 | 0.000044444444444444447 | 0.007584117 | 0.019788834 | 0.008662781 |

## Student counts, ledger and model verification

The last update contained **1,048,576** student decisions (`8 × 2048 × 64`): combat 793233, card reward 26135, map 49860, shop 15627, campfire 24790, and other 138931. Expert samples are excluded from these counts. The last update completed 1203 fresh episodes and 1440 midstart episodes; midstart mean floor was 36.082639.

The Act 2 ledger matched the merged eight ledger shards and contained **847 Act 2 entries**. Independently recomputed tied-rank Spearman correlations between value and final floor were **0.432734** at the Act 1 boss (954 cases) and **0.512961** at Act 2 entry (847 cases). These correlations apply to their recorded reached subsets, not to all starting games.

Ledger totals were **3355 card skips, 2727 card removals, 3257 rests and 2218 upgrades**. Encounter/elite fields retain their existing meaning as encountered formations, not individual kills.

The retained weights-only `dev-74.pt` loaded strictly on CPU with **12,435,074 parameters** and finite weights. SHA-256: `b46a16abb83e24d8dad2752ac0208abbe871f63ac23eacc5d9e2dd2b0a55b0a7` (49762853 bytes). Per-episode ledger SHA-256: `c496c54b2f665197a8a999a5a309e81b1c2d6b164377d80f48dc40bbce4e6f3a` (1639235 bytes). No model binary is published.

## Last five expert-round fields

The last five training rounds were **1593–1597**, all pure PPO on eight GPUs. In all five, beta was **0**, expert samples were **0**, and expert label collection was disabled. The remaining expert fields below follow the 70h table; a dash means not applicable or not computed while expert iteration is paused, rather than a copied historical value or a measured zero.

Both `expert_lane_memory_limit` and `expert_oom_recovered` events were checked in their intervals between successive worker-log update records. Neither event occurred.

| Update | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 1593 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |
| 1594 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |
| 1595 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |
| 1596 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |
| 1597 | — | — | 0 | — | — | — | — | — | — | — | — | — | — |

No supplemental 128-seed lookahead comparison is due at 74h. The completed 72h comparison is retained in the [72h report](e4-evaluation-72h.md); the next 8h comparison boundary is 80h. No ordinary evaluation, completed seed or training round was rerun for this audit.
