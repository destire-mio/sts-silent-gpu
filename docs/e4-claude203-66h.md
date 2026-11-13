# E4 evaluation at 66 training hours

Job 5639 completed the raw and EMA evaluations at natural checkpoint 2009, 237697.6190654753 training seconds and 526,647,202 cumulative training decisions. It resumed from update 2001, serial 209, with Adam step 32033 and EMA. That checkpoint retained six partial optimizer steps. Updates 1949–2001 on job 5636 and updates 2002–2009 on job 5639 used #203 with entropy 0.005. Both evaluations ran with key-last-chance enabled on source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use per-seed floor differences. Every denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.5029 | +1.5459 | 0.4580 | 448 / 238 / 338 |
| EMA | 33.4648 | +1.5078 | 0.4591 | 453 / 230 / 341 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 853/1024 | 843/1024 |
| Act 2 boss cleared | 376/1024 | 379/1024 |
| Act 3 first boss cleared | 103/1024 | 107/1024 |
| Act 3 second boss cleared | 34/1024 | 39/1024 |
| All three keys obtained | 255/1024 | 249/1024 |
| Act 4 entered | 34/1024 | 39/1024 |
| Shield and Spear defeated | 28/1024 | 27/1024 |
| Heart defeated | 4/1024 | 1/1024 |
| Ruby key | 265/1024 | 258/1024 |
| Emerald key | 389/1024 | 382/1024 |
| Sapphire key | 309/1024 | 293/1024 |
| Stalled | 1/1024 | 2/1024 |

EMA minus raw was -0.0381 floors with a paired SE of 0.4138 over 1,024 seeds. The independent CPU audit matched both sets of seed shards and ledgers, recorded the `dev-66.pt` and `dev-66-ema.pt` hashes, and reconciled the cumulative decision count from the 64-hour audit through both jobs.

The archived audit SHA256 is `68e70f796ceb862b62c09182f58110d771cb4c68403aea1523917ff1d13fd627`.
