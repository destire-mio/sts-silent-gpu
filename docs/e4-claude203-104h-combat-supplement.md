# E4 combat supplement at 104 training hours

Job 5751 evaluated the same raw `dev-104.pt` checkpoint in argmax and combat-lookahead modes on two V100s. The checkpoint was update 3131 at 374515.22890419513 training seconds and 820,772,737 cumulative decisions. Both modes shared the planned 128 seeds, 2,000,000,000 through 2,000,000,127. The independent CPU audit matched the model digest to the ordinary 104-hour audit and reconstructed timing from all eight logical shard receipts per mode.

Argmax recorded all 128 seeds in 102.546 seconds. The combat pass reached its one-hour cap at 3600.124 seconds and recorded 90 seeds. The 38 unfinished combat seeds are excluded from the paired statistics. The evaluation applied zero parameter updates.

| Mode | Paired seeds | Mean floor | Paired difference vs argmax | Paired SE |
| --- | ---: | ---: | ---: | ---: |
| Argmax | 90 / 128 | 32.3444 | 0.0000 | 0.0000 |
| Combat lookahead | 90 / 128 | 34.9000 | +2.5556 | 1.4372 |

On the 90 matched seeds, combat lookahead went farther in 46, tied in 22, and went fewer floors in 22.

| Milestone | Argmax | Combat lookahead |
| --- | ---: | ---: |
| Act 1 boss cleared | 76/90 | 78/90 |
| Act 2 boss cleared | 28/90 | 35/90 |
| Act 3 first boss cleared | 9/90 | 14/90 |
| Act 3 second boss cleared | 6/90 | 8/90 |
| All three keys at Act 3 | 0/90 | 0/90 |
| Act 4 entered | 0/90 | 0/90 |
| Shield and Spear defeated | 0/90 | 0/90 |
| Heart defeated | 0/90 | 0/90 |

The combat window logged 25,921 previewed decisions and 4,322 overrides, including work on unfinished games. The per-seed audit SHA256 is `bff10037d7234803f12eb57c3eedba49f5d730b1f0109ebefb96c84acd7bdef5`.
