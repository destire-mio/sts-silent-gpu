# E4 evaluation at 84 training hours

Job 5708 reached natural checkpoint 2543 at 302458.8213290153 training seconds and 666,632,090 cumulative training decisions. Job 5708 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.2510 | +1.2939 | 0.4491 | 461 / 218 / 345 |
| EMA | 33.4805 | +1.5234 | 0.4346 | 450 / 243 / 331 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 852/1024 | 875/1024 |
| Act 2 boss cleared | 358/1024 | 352/1024 |
| Act 3 first boss cleared | 113/1024 | 96/1024 |
| Act 3 second boss cleared | 33/1024 | 42/1024 |
| All three keys obtained | 244/1024 | 239/1024 |
| Act 4 entered | 33/1024 | 42/1024 |
| Shield and Spear defeated | 29/1024 | 36/1024 |
| Heart defeated | 2/1024 | 2/1024 |
| Ruby key | 255/1024 | 249/1024 |
| Emerald key | 383/1024 | 374/1024 |
| Sapphire key | 289/1024 | 289/1024 |
| Stalled | 1/1024 | 2/1024 |

EMA minus raw was +0.2295 floors with paired SE 0.3984. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `a4ad8e2acc1a667bbbc17ed0b90d67ab6b1b193d21b093f4c8dab8a6244296d1`.
