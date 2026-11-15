# E4 evaluation at 98 training hours

Job 5743 reached natural checkpoint 2957 at 352896.56143007753 training seconds and 775,159,691 cumulative training decisions. Job 5743 completed both evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. All 1,024 episodes contribute to each denominator.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.3672 | +0.4102 | 0.4451 | 426 / 236 / 362 |
| EMA | 33.2158 | +1.2588 | 0.4601 | 449 / 215 / 360 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 865/1024 | 854/1024 |
| Act 2 boss cleared | 313/1024 | 351/1024 |
| Act 3 first boss cleared | 101/1024 | 103/1024 |
| Act 3 second boss cleared | 30/1024 | 42/1024 |
| All three keys obtained | 219/1024 | 240/1024 |
| Act 4 entered | 30/1024 | 42/1024 |
| Shield and Spear defeated | 22/1024 | 33/1024 |
| Heart defeated | 4/1024 | 6/1024 |
| Ruby key | 221/1024 | 252/1024 |
| Emerald key | 345/1024 | 372/1024 |
| Sapphire key | 246/1024 | 288/1024 |
| Stalled | 3/1024 | 3/1024 |

EMA minus raw was +0.8486 floors with paired SE 0.4392. The independent CPU audit matched the original two-rank seed shards and ledgers, checked the baseline against the preceding audit and reconciled training decisions through update 2957.
The worker log records `engine_fault:852769260` on EMA seed 2,000,000,749, which is included in that mode's stalled count.

The archived audit SHA256 is `3e8a115e5e31213b89041a78c1749fcd82fe307aec2ba913c63645d427a6a0dc`.
