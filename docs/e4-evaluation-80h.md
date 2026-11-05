# E4 evaluation at 80 training hours

**From dev-70 rollback; expert v2 with the DDP ordering fix; 5 GPUs at evaluation.** The ordinary argmax evaluation completed at update **1761**, after **288169.9435521559** accumulated training seconds, using observation version **14**. Independent merging verified five result shards and five ledger shards for **1,024** full-run games, with fixed seeds 2000000000–2000001023 assigned as `seeds[rank::5]`. Every seed appeared exactly once.

| Metric | 70h | 78h | 80h |
| --- | ---: | ---: | ---: |
| Mean floor | 29.349609 | 29.886719 | 28.041992 |
| Act 1 boss passed | 810 | 825 | 800 |
| Act 2 boss passed | 205 | 218 | 164 |
| First Act 3 boss passed | 44 | 49 | 30 |
| Act 3 wins | 11 | 10 | 4 |
| Heart wins | 0 | 0 | 0 |

The paired mean floor difference from 78h was **−1.8447265625** (SE **0.3808880060010443**). Relative to retained dev-70 it was **−1.3076171875** (SE **0.4122089802281102**); relative to the fixed teacher baseline it was **+10.572265625** (SE **0.36574792919055366**). All comparisons use 1,024 matched seeds.

The score declined on this fixed seed set, by substantially more than the paired standard error relative to 78h. This does not isolate a causal effect of expert v2 or establish performance on other seeds. Settings remain unchanged under the #149 instruction.

There were **1019 deaths**, **4 Act 3 wins** and **1 stalled game** in the ordinary evaluation. Ruby, emerald and sapphire key counts were **17, 162 and 14**. One game collected all three keys, but no game entered Act 4, defeated Shield and Spear, or defeated the Heart. Collecting all three keys does not itself imply entering Act 4. Independent key/Act 4 checks found **0 contradictions**.

## Training interval and natural allocation change

The window `(280837.5640695497, 288169.9435521559]` contains **53 complete updates**, all on expert v2 with the DDP ordering fix, runtime source `a028153a61819c2c17671fe8552bfe3469f1f623`.

| Allocation | Included updates | GPUs | Complete updates | Applied optimizer steps | AMP skips |
| --- | --- | ---: | ---: | ---: | ---: |
| 5009 | 1709–1760 | 3 | 52 | 1248 | 0 |
| 5016 | 1761 | 5 | 1 | 40 | 0 |

The controller naturally saved allocation 5009 at checkpoint **1760**, training time **288042.02919142094**, serial **185**, and expanded to five GPUs. The resume retained Adam step **108073**, matching rank checkpoint metadata, with **0** retained partial optimizer steps and a reset incomplete collection cursor. The dev-70 rollback marker (update 1561) and expert-v2 startup marker (update 1688, cap 0.25) remained present; weights were not rolled back again.

There are **1288 applied optimizer steps**, **0 AMP skips** and **0 additional retained partial steps** in this interval. Partial steps 3 and 30 were already counted in the 78h interval; the earlier 16 was included in complete update 1691 and 49 in the 76h interval. None is added again.

The learning rate stayed at `0.000044444444444444447`, with **0** touches of the `1e-5` lower bound. KL ranged from **0.005838364475398539** to **0.010154051387820485** and ended at **0.010154051387820485**. All included complete updates reported finite parameters.

| Allocation / GPUs | LR minimum | LR maximum | KL minimum | KL maximum | Last KL |
| --- | ---: | ---: | ---: | ---: | ---: |

| 5009 / 3 | 0.000044444444444444447 | 0.000044444444444444447 | 0.005838364 | 0.008556140 | 0.008556140 |
| 5016 / 5 | 0.000044444444444444447 | 0.000044444444444444447 | 0.010154051 | 0.010154051 | 0.010154051 |

### Single-episode engine fault

Allocation 5009 recorded one training `episode_fault` after update **1730**, seed **1564686002**, reason `engine_fault:3830541970`, slot 777, with 1,304 recorded candidate indices. The training-window episode-fault counter sum is **1**. This is separate from the ordinary evaluation's stalled game.

The code maps uniquely to the actual v14 build's `core/state.hpp:71`, the `CardArena::add` assertion `index < card_capacity`; that build has **221 card-object slots**, including temporary and copied cards, not just the master deck. The precise originating card and whether the failure arose from live-object demand or a lifetime/reference issue remain unconfirmed. A checked CPU replay of the retained candidate sequence stopped earlier, at zero-based action 245, with `InvalidCandidate (0xe0000004)`; it did **not** reproduce the GPU capacity fault. An earlier unchecked loop is not valid reproduction evidence. No capacity or engine patch was deployed.

The trainer marked the affected episode as stalled, gave the faulting transition zero reward and excluded that transition through `action_valid=false`; it was not counted as a win. Earlier valid transitions may still affect learning. The allocation continued with finite updates, so this was not an allocation termination, NCCL failure, or GPU out-of-memory event. The original 4074-byte record was preserved with SHA-256 `fb8110cec0378566017f695a7f506b8d1e13227e021035763bfe2bf3eb194d19`.

## Student counts, ledger and retained model

The final update contained **655,360 student decisions** (`5 × 2048 × 64`): combat **450667**, card reward **19096**, map **39413**, shop **14402**, campfire **12666**, and other **119116**. Expert labels are excluded. It completed **488 fresh episodes** and **635 midstart episodes**, with midstart mean floor **34.97322834645669**. Training completion counts differ from the 1,024 full-run evaluation games.

The ledger matched all five ledger shards and contained **800 Act 2 entries**. Independently recomputed tied-rank Spearman correlations between recorded value and final floor were **0.4691486535076099** at the Act 1 boss (931 cases) and **0.4627936603750965** at Act 2 entry (800 cases). These are conditional on the reached recorded subsets.

Ledger totals were **1663 card skips, 2261 card removals, 3090 rests and 1897 upgrades**. Encounter/elite counts retain their meaning as encountered formations, not individual kills. Entry inventories, death details, outcomes, keys and final floors matched the per-game results.

The weights-only `dev-80.pt` loaded strictly on CPU with **12,435,074 parameters** and finite weights. SHA-256: `dd67f35c7d85de59d1ccd8a26dde187a0013b97f8dfaa6ec3094fa68c6cefea5` (49762853 bytes). Per-episode ledger SHA-256: `6285e041125530daf5a15d002610cbdf82662b149d590112e7a2360697c1b2af` (1604901 bytes). No model binary is published.

## Last five expert-round fields

Updates **1757–1760** used three GPUs; **1761** used five. Collection was enabled, beta absent (`null`), and the CE gradient cap **0.25**. All five rounds met the 0.30 expert overhead target. Allocated/projected/lane fields below are bytes; allocated peak is `expert_ppo_allocated_peak_bytes`.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |

| 1757 | 3 | 128 | 256 | 116 | 5.246799 | 3 | 0.205995 | 2.105198 | 0 | 5512553472 | 8783251200 | 15903392 | None | None |
| 1758 | 3 | 256 | 128 | 55 | 2.445074 | 1 | 0.210503 | 1.840009 | 0 | 5512553472 | 12053948928 | 17179816 | None | None |
| 1759 | 3 | 128 | 256 | 105 | 4.681303 | 3 | 0.212484 | 2.053861 | 0 | 5340599296 | 8639123968 | 16093136 | None | None |
| 1760 | 3 | 256 | 128 | 77 | 3.437624 | 1 | 0.210091 | 1.810654 | 0 | 5340599296 | 11937648640 | 17210252 | None | None |
| 1761 | 5 | 1 | 2 | 19 | 1.557357 | 64 | 0.212126 | 1.587093 | 0 | 5622616576 | 5648431954 | 52054016 | Ranks 0,1,2: 128→1 | None |

Between the preceding update records and update 1761, three `expert_lane_memory_limit` events were logged on ranks **0, 1 and 2**, reducing requested 128 lanes to 1. Their logged previous peaks were respectively **16800940032, 16769482752 and 16830300160 bytes**, against a growth limit of **15317729280 bytes**; projected allocated values were **8926984960, 8275707904 and 8471101696 bytes**. The logged reason was the existing allocated-memory gate with 1.5 GiB headroom. These protective lane reductions are not `expert_oom_recovered` events. No `expert_oom_recovered` event occurred in these five intervals; no memory policy was manually changed.

| Update | Override labels | Mean CE gradient norm | Mean PPO gradient norm | Mean CE scale | Combat entropy |
| ---: | ---: | ---: | ---: | ---: | ---: |

| 1757 | 116 | 62.935218493 | 0.662534673 | 0.002802461 | 0.325498839 |
| 1758 | 55 | 73.704366525 | 0.670575370 | 0.002545911 | 0.324473074 |
| 1759 | 105 | 65.277980248 | 0.666147251 | 0.002738370 | 0.322748910 |
| 1760 | 77 | 65.766548634 | 0.629071604 | 0.002552009 | 0.320690923 |
| 1761 | 19 | 86.054817390 | 0.760475371 | 0.750177721 | 0.322446085 |

Norms and scales are separate minibatch means; multiplying them does not give the mean applied CE norm or test the per-minibatch cap. In particular, sparse-label minibatches make the mean scale at update 1761 different from the preceding rounds. Zero argmax agreement describes the filtered override labels, not all unfiltered decisions.

RSS peaks for updates 1757–1760 were **2555908096, 2399612928 and 2409725952 bytes** across the three ranks. After the five-rank resume, update 1761 recorded **2338729984, 2327539712, 2328592384, 2323087360 and 2353848320 bytes**. These short windows and different allocations do not prove absence of long-term growth.

## Requested existing-log entropy baseline (#149)


The baseline uses the existing `entropy_by_decision_type.combat` values from allocation 4935, updates 1655–1688 (pure PPO, seven GPUs). All 34 rounds recorded this field; none is missing. No entropy was reconstructed and no training or evaluation was rerun. In the audited expert-v2 rounds 1704–1708, the explicit `combat_entropy` field equals this nested field exactly.

The unweighted mean of the 34 recorded round values is **0.29259318918149513**, with range **0.2821947829503164–0.3039758912158529**. The mean of the five recorded v2 round values at 1704–1708 is **0.31067125831988807**. These are per-round means, not a decision-count-weighted entropy recomputation. Different training times and GPU counts prevent this descriptive comparison from isolating a causal effect of v2.

| Update | Recorded combat entropy |
| ---: | ---: |
| 1655 | 0.2821947829503164 |
| 1656 | 0.28304423944221185 |
| 1657 | 0.285454067548012 |
| 1658 | 0.28647097878873407 |
| 1659 | 0.2849415042455617 |
| 1660 | 0.2838989677521076 |
| 1661 | 0.2862988071337979 |
| 1662 | 0.2878628414224799 |
| 1663 | 0.2878854277863153 |
| 1664 | 0.289991904862617 |
| 1665 | 0.2897688447375144 |
| 1666 | 0.2861884941452901 |
| 1667 | 0.28752440380662164 |
| 1668 | 0.29116773181903155 |
| 1669 | 0.2949567777970274 |
| 1670 | 0.29774587463709185 |
| 1671 | 0.3010071906565806 |
| 1672 | 0.29997860248231656 |
| 1673 | 0.30085583780790703 |
| 1674 | 0.29997015744609956 |
| 1675 | 0.3001364423131453 |
| 1676 | 0.3039758912158529 |
| 1677 | 0.2995195703387364 |
| 1678 | 0.29758554882362404 |
| 1679 | 0.2984581551561803 |
| 1680 | 0.2990835308141374 |
| 1681 | 0.2937025689295485 |
| 1682 | 0.29595282269664286 |
| 1683 | 0.2957577793104651 |
| 1684 | 0.28968647856842494 |
| 1685 | 0.2915670585471583 |
| 1686 | 0.29058994689173573 |
| 1687 | 0.29152847181768365 |
| 1688 | 0.2934167294798636 |


For the current last five rounds, 1757–1761, the mean of the recorded combat-entropy values is **0.32317156622520177**. The comparison with the earlier pure-PPO baseline is descriptive: training time, weights, GPU counts and the allocation restart also differ. The existing 34 baseline rows were reused; they were not re-extracted or recomputed for this report.

## 128-seed combat-preview comparison

The natural comparison has ended under the shared **3600-second combat-mode cap**. Argmax completed **128/128** seeds; combat completed **122/128**, with status `combat_one_hour_cap`. All outcomes below were independently audited from the existing receipts. Completed seeds were not rerun.

Both modes loaded the retained dev-80 checkpoint, SHA-256 `dd67f35c7d85de59d1ccd8a26dde187a0013b97f8dfaa6ec3094fa68c6cefea5`. The comparison used the first 128 fixed seeds, `2000000000..2000000127`, split into eight logical shards over the actual GPU workers. It performed no parameter updates. The later dev-78 training rollback did not change this evaluation checkpoint.

The paired result uses the **122-seed completed intersection**:

| Metric | Argmax, matched 122 | Combat preview, matched 122 |
|---|---:|---:|
| Mean floor | 27.450819672131146 | 30.24590163934426 |
| Act 1 boss reached | 95 | 103 |
| Act 2 boss reached | 15 | 19 |
| Act 3 first boss reached | 1 | 4 |
| Act 3 victories | 0 | 0 |
| Heart wins | 0 | 0 |

The paired mean floor difference is **+2.7950819672131146**, with paired SE **0.8207393810668202**. The six excluded, unfinished combat seeds are `2000000014, 2000000022, 2000000046, 2000000094, 2000000110, 2000000126`. These time-censored exclusions make this a result conditional on completed pairs; it does not establish a gain over all 128 seeds or an effect of expert-v2 training.

For context, argmax over all 128 seeds had mean floor **28.1328125**, Act 1/2/3-first-boss counts **100/19/4**, and zero Act 3 or Heart victories. Its complete-set mean must not be substituted for the matched 122-seed baseline. All 128 argmax and 122 completed combat episodes ended in death; there were no stalled outcomes or progression/key contradictions among those completed episodes. Argmax/combat collected ruby/emerald/sapphire keys in **3/22/4** and **2/21/4** completed episodes, respectively; neither reached Act 4, Shield/Spear or the Heart.

### Physical-worker timing

Argmax used **39.129040293977596 seconds**. Combat used **3600.342690088786 seconds** across the natural windows. Both totals include initialization. For each window, serial logical-shard times were summed within each physical worker, the maximum worker sum was taken, and those window maxima were added. Taking a maximum over the eight logical shards would be incorrect.

| Combat natural window | Physical GPUs | Maximum physical-worker serial sum, seconds |
|---|---:|---:|
| 1 | 5 | 201.99315792194102 |
| 2 | 6 | 514.1281365589821 |
| 3 | 7 | 502.2379268720397 |
| 4 | 6 | 502.7986266039661 |
| 5 | 5 | 520.9844725389848 |
| 6 | 4 | 524.6688684679684 |
| 7 | 3 | 833.1503582429141 |
| 8 | 3 | 0.3811428819899447 |

All timer prefixes are final and have `failed=false`; the short final window is included. The combat result is capped and has `complete=false`, while the final comparison report exists. It is not a 128-game combat completion or a training-job failure. These mode times are diagnostics; allocation cost remains the original controller's single Slurm settlement.

### Preview and override counters

| Counter | Argmax | Combat preview |
|---|---:|---:|
| Mainline decisions | 65940 | 176421 |
| Preview decisions | 0 | 74557 |
| Overrides | 0 | 13001 |
| Preview engine steps | 0 | 48365305 |
| Preview decisions / mainline decisions | 0 | 0.42260841963258344 |
| Overrides / mainline decisions | 0 | 0.07369304107787622 |
| Overrides / preview decisions | 0 | 0.1743766514210604 |

Counters cover **all attempted work**, including unfinished prefixes and natural-window retries, whereas the outcome comparison covers only the completed intersection. They must not be described as counters restricted to the 122 paired games.

Independent supplemental audit: **91 source/receipt files** checked, including unique seed ownership, checkpoint identity, completed intersection and exclusions, outcome/progression consistency, counters, final window receipts, physical-worker mapping and serial timing, and agreement with the natural report. Audit artifact SHA-256: `0508e11a162b7d8639242e0dcc27b35ab0eb61452717c8b5ae74c2e0e19c6a81`. The ordinary 1,024-game audit above was not repeated.

After the comparison ended, the retained-clock dev-78 rollback branch resumed pure PPO. Its first new completed update, **1762 / 288277.9648657319 training seconds**, used **3 GPUs**, **393216 = 3 × 2048 × 64** student decisions and **24 applied optimizer steps**, with finite parameters, **0 AMP skips**, `expert_beta=0`, `expert_samples=0`, and `expert_collection=false`. The one-time rollback marker had already been verified in a natural checkpoint; these observations close the requested startup/finite-update/checkpoint acceptance. They do not certify future fault-free operation.

Next ordinary evaluation: **82h = 295200 seconds**, with the learning-rate window strictly greater than **288169.9435521559**. No evaluation games were rerun for this audit.

Independent ordinary-audit artifact SHA-256: `c752394eb2a542eac5193e2b494271b38b0ce0093a1bb864e2f814b6c7ac6c4b`; audit script SHA-256: `f42f58dbcbaeda8c13e2db2c2f783bb587f6a6be99266e10c46c990b6ea8e304`.

