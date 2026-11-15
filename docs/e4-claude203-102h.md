# E4 evaluation at 102 training hours

Job 5745 reached natural checkpoint 3073 at 367272.1104665848 training seconds and 805,568,389 cumulative training decisions. Job 5745 completed both evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. All 1,024 episodes contribute to each denominator.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.5283 | +1.5713 | 0.4452 | 455 / 218 / 351 |
| EMA | 33.8652 | +1.9082 | 0.4460 | 477 / 213 / 334 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 869/1024 | 865/1024 |
| Act 2 boss cleared | 361/1024 | 374/1024 |
| Act 3 first boss cleared | 87/1024 | 118/1024 |
| Act 3 second boss cleared | 36/1024 | 42/1024 |
| All three keys obtained | 247/1024 | 261/1024 |
| Act 4 entered | 35/1024 | 42/1024 |
| Shield and Spear defeated | 28/1024 | 31/1024 |
| Heart defeated | 2/1024 | 2/1024 |
| Ruby key | 256/1024 | 272/1024 |
| Emerald key | 382/1024 | 382/1024 |
| Sapphire key | 302/1024 | 304/1024 |
| Stalled | 2/1024 | 1/1024 |

EMA minus raw was +0.3369 floors with paired SE 0.4012. The independent CPU audit matched the original two-rank seed shards and ledgers, checked the baseline against the preceding audit and reconciled training decisions through update 3073.
The worker log records `engine_fault:852769260` on RAW seed 2,000,000,749, which is included in that mode's stalled count.

The archived audit SHA256 is `c16953c24a58bdf880c8fa817c6eb1d3095e4a177b7af2b4bddc92be556f277e`.
