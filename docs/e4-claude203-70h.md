# E4 evaluation at 70 training hours

Job 5641 completed the raw and EMA evaluations at natural checkpoint 2127, 252017.0358043136 training seconds and 557,580,191 cumulative training decisions. It resumed from update 2105, serial 220, with Adam step 33691 and EMA. Updates 2069–2105 on job 5640 and updates 2106–2127 on job 5641 used #203 with entropy 0.005. Both evaluations ran with key-last-chance enabled on source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use per-seed floor differences. Every denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.0283 | +1.0713 | 0.4479 | 427 / 237 / 360 |
| EMA | 33.5684 | +1.6113 | 0.4407 | 435 / 236 / 353 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 867/1024 | 867/1024 |
| Act 2 boss cleared | 338/1024 | 361/1024 |
| Act 3 first boss cleared | 98/1024 | 106/1024 |
| Act 3 second boss cleared | 31/1024 | 26/1024 |
| All three keys obtained | 227/1024 | 253/1024 |
| Act 4 entered | 31/1024 | 25/1024 |
| Shield and Spear defeated | 23/1024 | 19/1024 |
| Heart defeated | 2/1024 | 2/1024 |
| Ruby key | 243/1024 | 258/1024 |
| Emerald key | 372/1024 | 412/1024 |
| Sapphire key | 277/1024 | 300/1024 |
| Stalled | 0/1024 | 3/1024 |

EMA minus raw was +0.5400 floors with a paired SE of 0.4270 over 1,024 seeds. The independent CPU audit matched both sets of seed shards and ledgers, recorded the `dev-70.pt` and `dev-70-ema.pt` hashes, and reconciled the cumulative decision count from the 68-hour audit through both jobs.

The archived audit SHA256 is `e06a62d30f3aabb2241d88559ee77efeaa0bef6b417f044612fa8b1a0534e2ce`.
