### Dev-64 fixed-seed lookahead window: combat capped before completion

This evaluation-only window used the retained dev-64.pt checkpoint, SHA-256 64ac4d81d9e16b392977661b421fdd3a36ab3b7b68968b3b10a93921dd2acf1d, on fixed dev seeds 2000000000–2000000255. The dedicated argmax baseline completed all 256 games. Combat reached the existing one-hour recorded-mode limit with 255 completed games; seed 2000000221 remained unfinished. The raw report records combat complete=false and skipped=combat_one_hour_cap.

**There is no complete 256-game combat/argmax quality comparison.** No combat mean, full-set paired delta/SE, full-set Boss/win comparison or complete-mode speed ratio is claimed. The retained receipts are not used to silently drop the unfinished seed or to substitute an ordinary 1024-game baseline.

| Mode | Completed games | Mean floor | Act1Boss | Act2Boss | First Act3Boss | Act3 wins | Heart wins |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Dedicated argmax | 256/256 | 30.16015625 | 209 | 50 | 9 | 2 | 0 |
| Combat | 255/256, capped | — | — | — | — | — | — |

The argmax metrics were independently recomputed from eight × 32 per-seed receipts. All 16 mode/rank receipts carry the same checkpoint SHA; completed seeds are unique and assigned to the expected ranks. Combat rank counts are 32/32/32/32/32/31/32/32. Act3 wins use reach.act3_second_boss, and Heart wins use reach.heart.

| Mode | Recorded mode-window wall, seconds | Interpretation |
| --- | ---: | --- |
| Argmax | 54.166882202989655 | Complete 256-game baseline, including initialization. |
| Combat | 3600.006213340006 | Existing cap reached; 255 completed games plus unfinished work, not a full-256 timing. |

Combat ran over two natural execution windows. Their maximum per-rank recorded walls are 1294.3865775879967 and 2305.6196357520093 seconds; their sum independently matches the report. The first window ended with a normal save/yield, and continuation skipped every completed seed. These mode timers do not include the entire intervening scheduling wait or all synchronization/report overhead. An interrupted non-final crash prefix would be only a durable lower bound; no unrecorded crash-prefix time is invented here.

| Mode | Persisted mainline decisions | Preview decisions | Overrides | Preview steps | Previews / mainline | Overrides / mainline | Overrides / previews |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Argmax | 147131 | 0 | 0 | 0 | 0 | 0 | 0 |
| Combat, capped window | 207950 | 94199 | 15119 | 69172417 | 0.45298869920654006 | 0.07270497715797067 | 0.1605006422573488 |

Counters were independently summed from durable receipts. They describe persisted decisions, including recorded unfinished-game prefixes, and are not counters for a completed 256-game combat set. No lost or unpersisted work is reconstructed.

The report records evaluation_only=true and parameter_updates=0. The prepared evaluator uses a separate policy with gradients disabled and forked RNG restoration. This documents that execution path and does not claim an independent before/after hash of every rank's training tensors. Raw report SHA-256: 280f8d63265039e52f90e4d5080d92c003f618352a88c810f58482342ceff30a. The original report and 16 receipts remain retained; an existing report makes future startup skip this window, including the capped unfinished seed.

After the capped report was written, ordinary PPO resumed and the separately authorized expert iteration began. This dev-64 comparison contains no expert-training effect measurement. Its first ordinary post-expert examination remains pending. The earlier dev-60 dedicated argmax/ordinary-first-256 discrepancy on two seeds remains unresolved and is not chased or rerun. The ordinary 64h 1024-game examination remains separately documented in [the 64h report](e4-evaluation-64h.md).

### Matched completed-seed supplement (#142)

Per the subsequent authorization, this table excludes seed 2000000221 from **both** modes and independently recomputes the remaining 255 matched games. No game was rerun, no timer was reset, and the original capped report is unchanged. This is a completed-seed subset, not the full 256-game comparison. Selection by completion can bias the comparison; these fixed-dev numbers do not establish unseen-seed generalization or a causal training effect.

| Mode, same 255 seeds | Mean floor | Act1Boss /255 | Act2Boss /255 | First Act3Boss /255 | Act3 wins /255 | Heart wins /255 | Paired floor delta vs argmax | Paired SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Argmax | 30.08235294117647 | 208 | 49 | 9 | 2 | 0 | 0 | 0 |
| Combat | 34.88235294117647 | 233 | 81 | 27 | 6 | 0 | 4.8 | 0.6861833680849498 |

Future eight-hour comparison boundaries request the first 128 seeds, 2000000000–2000000127, with the same one-hour combat cap. If incomplete, their report identifies excluded seeds and pairs both modes on the same completed subset. Ordinary 1024-game examinations, expert-training settings and the current running job are unchanged; this takes effect at the next natural resume.
