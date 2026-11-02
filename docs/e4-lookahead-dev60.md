### Dev-60 fixed-seed lookahead comparison

All three modes completed the same 256 fixed dev seeds, 2000000000–2000000255, with eight ranks × 32 games, fresh starts and the same retained dev-60.pt checkpoint. SHA-256: 2bdf1258e7900458d71d4902f599c7b6483309cc4570c15e8cf464267b87b40e. This is the limited evaluation-only comparison authorized in C137; it does not enable training search or the 512 model.

| Mode | Mean floor | Act1Boss /256 | Act2Boss /256 | First Act3Boss /256 | Act3 wins /256 | Heart wins /256 | Paired floor delta vs argmax | Paired SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| argmax | 30.71875 | 214 | 54 | 8 | 1 | 0 | 0 | 0 |
| turn | 33.51953125 | 225 | 81 | 16 | 1 | 0 | 2.80078125 | 0.730095764977333 |
| combat | 35.6328125 | 231 | 101 | 23 | 5 | 0 | 4.9140625 | 0.7371666471503956 |

Act3 wins use RewardLedger.reach.act3_second_boss; Heart wins use reach.heart. Paired statistics were independently recomputed seed by seed against this dedicated timed argmax baseline. All mode/rank files have exactly the required 32 seeds, with 256 unique seeds per mode. These fixed-dev results do not establish unseen-seed generalization, and they do not authorize a training-policy change.

| Mode | Recorded mode-window wall, seconds | Timing interpretation |
| --- | ---: | --- |
| argmax | 53.340614850996644 | Complete fresh 256-game baseline, directly timed; not one quarter of the ordinary 1024-game evaluation. |
| turn | 399.22412185301073 | Normal continuation window for the remaining games; excludes missing exact durations from earlier failed attempts. |
| combat | 1748.9919374320016 | Complete 256-game mode window; no combat seed was replayed after completion. |

The timers record the sum of max-rank wall durations per normally completed evaluation window, including mode setup. Turn had already persisted 21 completed games before a failed attempt. Its exact total elapsed mode time cannot be recovered: the recorded normal window is a lower bound, 399.22412185301073 seconds; adding both earlier failed allocations in full gives a conservative upper bound of 1013.2241218530107 seconds, which also includes non-turn initialization and baseline work. This interval is not an exact timing measurement. No complete turn/argmax speed ratio is claimed.

The runner naturally admitted combat using four times the recorded turn window, 1596.896487412043 seconds, below its 3600-second threshold. This estimate omitted the unavailable failed-turn durations; that limitation remains part of this result. Combat retained its own 3600-second limit and completed without being skipped.

| Mode | Persisted mainline decisions | Preview decisions | Overrides | Preview steps | Previews / mainline | Overrides / mainline | Overrides / previews |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| argmax | 146270 | 0 | 0 | 0 | 0 | 0 | 0 |
| turn | 215742 | 84556 | 22809 | 13349146 | 0.3919311028914166 | 0.10572350307311511 | 0.2697502247031553 |
| combat | 169552 | 68276 | 10496 | 47295387 | 0.40268472209115785 | 0.06190431254128527 | 0.15372898236569218 |

Counters and fractions were independently summed and recomputed from durable mode/rank receipts. They cover persisted decisions, including any restarted unfinished prefixes already captured in those counters. Unpersisted work lost in the earlier failures cannot be reconstructed, so these are not a claim to count every decision spent across failed attempts. The conditional overrides/previews fraction is kept separate from overrides/all mainline decisions.

Two compatibility corrections were needed before completion: UInt32 fault-flag indexing and the exclusion of preview cache slots from mainline heartbeat provenance. Candidate scoring, preview depths, masks, training reward, learner and model settings were unchanged. All eight actual heartbeat streams subsequently contained mainline lanes only. A later NCCL collective timed out at 600 seconds after per-game receipts had been preserved; the natural continuation produced the final report from completed receipts without rerunning completed seeds, then resumed finite PPO updates. The table's mode-window timers do not measure all synchronization, restart or report-generation overhead.

The dedicated argmax baseline's durable files are byte-for-byte unchanged from their original completed run. Its floor/reach records differ from the ordinary 60h evaluation on two seeds, 2000000120 and 2000000157; their cause has not been established. Consequently, the paired table uses the dedicated 256-game baseline throughout and does not substitute the ordinary 1024-game result or assert bitwise reproduction between those evaluation batch layouts.

The comparison used a separate evaluation policy with gradients disabled and forked CPU/CUDA RNG restoration. The recorded comparison has zero parameter updates, and the allocations that collected the game receipts contain no PPO update records. This verifies the evaluation-only execution path; it is not an independent before/after hash proof of every rank's training tensors. The raw report and all 24 mode/rank receipts remain retained; raw report SHA-256 ec63ec198c76f78ef505a5148d9d15186f2b0832630dcb7019ad07c79bfd0edf. No model binary or private operations/accounting data is published. Ordinary 60h evaluation remains separately documented in [the 60h report](e4-evaluation-60h.md).
