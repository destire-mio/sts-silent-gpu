# E4 evaluation at 68 training hours

Job 5640 completed the raw and EMA evaluations at natural checkpoint 2068, 244830.51880552608 training seconds and 542,113,697 cumulative training decisions. It resumed from update 2053, serial 215, with Adam step 32869 and EMA. That checkpoint retained 10 partial optimizer steps. Updates 2010–2053 on job 5639 and updates 2054–2068 on job 5640 used #203 with entropy 0.005. Both evaluations ran with key-last-chance enabled on source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use per-seed floor differences. Every denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.7637 | +0.8066 | 0.4727 | 422 / 233 / 369 |
| EMA | 33.9131 | +1.9561 | 0.4733 | 462 / 210 / 352 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 834/1024 | 856/1024 |
| Act 2 boss cleared | 357/1024 | 393/1024 |
| Act 3 first boss cleared | 101/1024 | 117/1024 |
| Act 3 second boss cleared | 28/1024 | 40/1024 |
| All three keys obtained | 244/1024 | 260/1024 |
| Act 4 entered | 28/1024 | 40/1024 |
| Shield and Spear defeated | 20/1024 | 34/1024 |
| Heart defeated | 1/1024 | 2/1024 |
| Ruby key | 253/1024 | 267/1024 |
| Emerald key | 384/1024 | 412/1024 |
| Sapphire key | 308/1024 | 324/1024 |
| Stalled | 0/1024 | 1/1024 |

EMA minus raw was +1.1494 floors with a paired SE of 0.4476 over 1,024 seeds. The independent CPU audit matched both sets of seed shards and ledgers, recorded the `dev-68.pt` and `dev-68-ema.pt` hashes, and reconciled the cumulative decision count from the 66-hour audit through both jobs.

The archived audit SHA256 is `d453b059cf51ce2fe28550eb1f32cca665b9eb33ea462f4803e5f10bd8573e4b`.
