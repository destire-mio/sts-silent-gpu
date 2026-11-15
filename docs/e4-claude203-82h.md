# E4 evaluation at 82 training hours

Job 5690 reached natural checkpoint 2484 at 295287.47236405616 training seconds and 651,165,594 cumulative training decisions. Job 5690 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.3750 | +0.4180 | 0.4607 | 423 / 229 / 372 |
| EMA | 33.5986 | +1.6416 | 0.4393 | 455 / 224 / 345 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 850/1024 | 853/1024 |
| Act 2 boss cleared | 331/1024 | 368/1024 |
| Act 3 first boss cleared | 82/1024 | 97/1024 |
| Act 3 second boss cleared | 29/1024 | 32/1024 |
| All three keys obtained | 230/1024 | 254/1024 |
| Act 4 entered | 29/1024 | 32/1024 |
| Shield and Spear defeated | 29/1024 | 27/1024 |
| Heart defeated | 3/1024 | 4/1024 |
| Ruby key | 237/1024 | 267/1024 |
| Emerald key | 385/1024 | 394/1024 |
| Sapphire key | 280/1024 | 327/1024 |
| Stalled | 4/1024 | 1/1024 |

EMA minus raw was +1.2236 floors with paired SE 0.4411. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `7289c2b1a1de3035e6d715b41eb5a12b2815ce74449bb90f28fad5e365249f02`.
