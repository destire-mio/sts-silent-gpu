# E4 evaluation at 78 training hours

**From dev-70 rollback; expert v2 with the DDP ordering fix; 3 GPUs at evaluation.** The ordinary argmax evaluation completed at update **1708**, after **280837.5640695497** accumulated training seconds, with observation version **14**. All **1,024** full-run games used fixed seeds 2000000000–2000001023, assigned as `seeds[rank::3]` across three physical ranks. Independent merging verified the three result shards and three ledger shards, with every seed present exactly once.

| Metric | 70h | 76h | 78h |
| --- | ---: | ---: | ---: |
| Mean floor | 29.349609 | 29.809570 | 29.886719 |
| Act 1 boss passed | 810 | 826 | 825 |
| Act 2 boss passed | 205 | 218 | 218 |
| First Act 3 boss passed | 44 | 54 | 49 |
| Act 3 wins | 11 | 13 | 10 |
| Heart wins | 0 | 0 | 0 |

The paired mean floor difference from 76h was **+0.0771484375** (SE **0.4093561992390206**). Relative to retained dev-70, it was **+0.537109375** (SE **0.42150016366658166**); relative to the fixed teacher baseline it was **+12.4169921875** (SE **0.3865534315307717**). Every comparison uses all 1,024 matched seeds.

The 76h-to-78h mean difference is small relative to its paired standard error. This fixed seed set does not establish a training benefit from expert v2 or isolate its causal effect. The interval includes pure PPO, expert-v2 activation, a communication fix and changing GPU counts, while retaining optimizer state from the earlier dev-70 weight rollback.

There were **1013 deaths**, **10 Act 3 wins** and **1 stalled game**. Ruby, emerald and sapphire key counts were **20, 162 and 2**. No game collected all three keys, entered Act 4, defeated Shield and Spear, or defeated the Heart. Independent key/Act 4 checks found **0 contradictions**.

## Training window, runtime changes and recovery

The window `(273692.74861606734, 280837.5640695497]` contains **54 complete updates** on the retained trajectory.

| Allocation | Included updates | Phase / runtime | GPUs | Complete updates | Logged applied optimizer steps | AMP skips |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| 4935 | 1655–1688 | Pure PPO | 7 | 34 | 1904 | 0 |
| 4937 | 1689–1690 | Expert v2, initial runtime | 6 | 2 | 90 | 6 |
| 4994 | 1691–1693 | Expert v2, DDP fix | 6 | 3 | 144 | 0 |
| 4995 | 1694–1695 | Expert v2, DDP fix | 5 | 2 | 80 | 0 |
| 5003 | 1696–1698 | Expert v2, DDP fix | 4 | 3 | 96 | 0 |
| 5009 | 1699–1708 | Expert v2, DDP fix | 3 | 10 | 240 | 0 |

Expert v2 activated once at update 1688, retaining current weights, Adam, scaler, value calibration, pools, RNG and accumulated training time. It collects only overrides with preview work, a selected action different from the first candidate, and paired mean improvement greater than its paired standard error. CE has no beta weight: its separately accumulated gradient is averaged across ranks and capped at 0.25 of the PPO gradient norm before the original AMP unscale, clipping and Adam step. The earlier dev-70 rollback marker at update 1561 remains preserved; no second weight rollback occurred.

Update **1689** had **6 AMP skips** and a logged non-finite CE gradient norm while parameters remained finite; only **42** of its 48 optimizer attempts applied. Update 1690 was finite with 48 applied steps and no skips. This historical diagnostic is retained in the interval.

Allocations **4955** and **4971** failed after resuming checkpoint 1690, without producing a new complete update. Their NCCL collective mismatch was reproduced with a real two-rank, two-optimizer-step CPU regression: uneven CE minibatches entered DDP forward bucket-rebuild broadcasts while zero-label ranks entered a different collective. The focused fix uses the underlying module for CE forward within the existing `no_sync` region, while retaining the original PPO DDP path and gradient merge. Eleven targeted CPU checks passed after the fix. The fix was [published separately](https://github.com/destire-mio/sts-silent-gpu/commit/51e04d58773f01e3da5b8a630e4b624c003e9a24) and naturally entered allocation 4994 as runtime source `a028153a61819c2c17671fe8552bfe3469f1f623`. It completed finite updates 1691–1693 without AMP skips and naturally saved a checkpoint carrying the existing markers. Failed allocations still incur their actual allocation costs; they add no completed training rows here.

The complete rows contain **2554 applied optimizer steps** and **6 AMP skips**. Retained partial steps **3** at the 7-to-6 transition and **30** at the 5-to-4 transition add **33** steps once, for **2587 applied optimizer steps** in this interval. The same-world six-rank resume of checkpoint 1690 retained cursor **16**, already included in the complete update 1691 row: updates 1691–1693 log 144 steps while Adam advances only 128 after that resume, exactly `144 − 16`. The repeated retained-16 metadata on failed 4955/4971 resumes is therefore not added again. The earlier partial 49 was counted in the 76h interval and is excluded here. The 4-to-3 transition retained no additional partial steps.

The learning rate stayed at `0.000044444444444444447`, with **0** touches of the `1e-5` lower bound. KL ranged from **0.005088497830087972** to **0.009115301237752246** and ended at **0.007200153939469045**. All included complete updates reported finite parameters.

| Allocation / GPUs | LR minimum | LR maximum | KL minimum | KL maximum | Last KL |
| --- | ---: | ---: | ---: | ---: | ---: |
| 4935 / 7 | 0.000044444444444444447 | 0.000044444444444444447 | 0.005088498 | 0.008647320 | 0.007811992 |
| 4937 / 6 | 0.000044444444444444447 | 0.000044444444444444447 | 0.007967614 | 0.009115301 | 0.009115301 |
| 4994 / 6 | 0.000044444444444444447 | 0.000044444444444444447 | 0.006941232 | 0.008729173 | 0.008729173 |
| 4995 / 5 | 0.000044444444444444447 | 0.000044444444444444447 | 0.008465774 | 0.008637350 | 0.008465774 |
| 5003 / 4 | 0.000044444444444444447 | 0.000044444444444444447 | 0.006141140 | 0.007490577 | 0.006141140 |
| 5009 / 3 | 0.000044444444444444447 | 0.000044444444444444447 | 0.005931570 | 0.007200154 | 0.007200154 |

## Student counts, ledger and retained model

The final update contained **393,216 student decisions** (`3 × 2048 × 64`): combat **295810**, card reward **10100**, map **19052**, shop **5821**, campfire **8985**, and other **53448**. Expert samples are excluded. It completed **490 fresh episodes** and **560 midstart episodes**, with midstart mean floor **36.816071428571426**. These training completion counts differ from the 1,024 full-run evaluation games.

The ledger matched all three ledger shards and contained **825 Act 2 entries**. Independently recomputed tied-rank Spearman correlations between recorded value and final floor were **0.4291132582334184** at the Act 1 boss (946 cases) and **0.4659460606808215** at Act 2 entry (825 cases). These correlations are conditional on the recorded reached subsets.

Ledger totals were **2518 card skips, 2590 card removals, 3214 rests and 2152 upgrades**. Encounter/elite counts retain their existing meaning as encountered formations, not individual kills. Entry inventories, death details, outcomes, keys and final floors matched the per-game results.

The weights-only `dev-78.pt` loaded strictly on CPU with **12,435,074 parameters** and finite weights. SHA-256: `54fc4a7ea0bdf0840fe155cb63c6284e9a4957af813eeb2dd47975ae31dfdb28` (49762853 bytes). Per-episode ledger SHA-256: `40d5d6ec3552ea917a36691df52df817d4cb96c8bdb3690dfad8592979df23da` (1633368 bytes). No model binary is published.

## Last five expert-round fields

Updates **1704–1708** all used expert v2 on three GPUs. Collection was enabled, beta was absent (`null`), and the CE gradient cap was **0.25**. Both `expert_lane_memory_limit` and `expert_oom_recovered` events were checked between successive worker-log update records; neither occurred in these five intervals. The earlier NCCL failures are described separately above. Allocated/projected/lane memory fields below are bytes; allocated peak is the logged `expert_ppo_allocated_peak_bytes`.

| Update | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 1704 | 32 | 32 | 71 | 3.115765 | 8 | 0.209174 | 2.051519 | 0 | 5073742848 | 6098569728 | 17894320 | None | None |
| 1705 | 32 | 32 | 76 | 3.308674 | 11 | 0.201852 | 2.063084 | 0 | 5340311552 | 6365138432 | 19665280 | None | None |
| 1706 | 32 | 32 | 76 | 3.190382 | 9 | 0.216599 | 2.153072 | 0 | 5340311552 | 6365138432 | 20383616 | None | None |
| 1707 | 32 | 32 | 74 | 3.203689 | 10 | 0.218384 | 2.575830 | 0 | 5092605952 | 6071019520 | 19279280 | None | None |
| 1708 | 32 | 64 | 97 | 4.308819 | 8 | 0.207491 | 2.309104 | 0 | 5559933440 | 6538347008 | 17418384 | None | None |

The zero agreement values describe the filtered override labels, which deliberately disagree with the first-candidate action; they are not the unfiltered action agreement rate. Expert overhead stayed below its 0.30 target in these five rounds.

| Update | Override labels | Mean CE gradient norm | Mean PPO gradient norm | Mean CE scale | Combat entropy |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1704 | 71 | 70.171516101 | 0.893188506 | 0.004060259 | 0.309649254 |
| 1705 | 76 | 60.554938714 | 0.608701345 | 0.002849996 | 0.309031606 |
| 1706 | 76 | 68.390207529 | 0.589261737 | 0.002302847 | 0.309653781 |
| 1707 | 74 | 67.573189497 | 0.711272005 | 0.002710274 | 0.312738076 |
| 1708 | 97 | 60.281634331 | 0.781531089 | 0.003588933 | 0.312283574 |

Gradient norms and scale are separate minibatch means. Their product is not the mean applied CE gradient norm and cannot be used to test the per-minibatch cap. Final-update per-rank RSS peaks were **2400370688, 2366930944 and 2378317824 bytes**; this short observation does not establish absence of long-term memory growth.

No supplemental 128-seed comparison is due at 78h. The next 8h comparison boundary is **80h**, using the first 128 fixed seeds' completed intersection, eight logical shards shared across the actual GPU workers, and the existing per-mode time cap. Next ordinary evaluation: **80h = 288000 seconds**, with the learning-rate window strictly greater than **280837.5640695497**. Completed evaluation seeds were not rerun for this audit.

Independent audit artifact SHA-256: `6a5c29a108add5b8589465531fce068cefb2cfaa39c65ed5cdd6d12955b208ea`; audit script SHA-256: `9f8706ad2aaad750fd21e28f2c637024d478c9bb487d48c2b6d07fa97060b263`.


