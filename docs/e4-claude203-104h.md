# E4 evaluation at 104 training hours

Job 5751 reached natural checkpoint 3131 at 374515.22890419513 training seconds and 820,772,737 cumulative training decisions. Job 5751 completed both evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. All 1,024 episodes contribute to each denominator.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.6221 | +0.6650 | 0.4645 | 432 / 210 / 382 |
| EMA | 33.5859 | +1.6289 | 0.4724 | 471 / 197 / 356 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 834/1024 | 847/1024 |
| Act 2 boss cleared | 348/1024 | 376/1024 |
| Act 3 first boss cleared | 107/1024 | 126/1024 |
| Act 3 second boss cleared | 37/1024 | 47/1024 |
| All three keys obtained | 251/1024 | 266/1024 |
| Act 4 entered | 37/1024 | 47/1024 |
| Shield and Spear defeated | 33/1024 | 41/1024 |
| Heart defeated | 3/1024 | 7/1024 |
| Ruby key | 263/1024 | 283/1024 |
| Emerald key | 406/1024 | 394/1024 |
| Sapphire key | 292/1024 | 315/1024 |
| Stalled | 0/1024 | 4/1024 |

EMA minus raw was +0.9639 floors with paired SE 0.4170. The independent CPU audit matched the original two-rank seed shards and ledgers, checked the baseline against the preceding audit and reconciled training decisions through update 3131.
The worker log records `engine_fault:852769260` on EMA seed 2,000,000,749, which is included in that mode's stalled count.

The archived audit SHA256 is `a281a6eaf59e551559cdeb7429e1c7235ef864698772afc60f8e2238fc18bdb7`.
