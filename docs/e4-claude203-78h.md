# E4 evaluation at 78 training hours

Job 5678 evaluated raw and EMA weights at natural checkpoint 2366, 280844.98803345626 training seconds and 620,232,605 cumulative training decisions. Job 5677 ran updates 2308–2338. Job 5678 resumed from update 2338, serial 245, preserving EMA and Adam step 37425, including 9 saved partial optimizer steps. It ran updates 2339–2366. Both jobs used #203 with entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use per-seed floor differences. Every denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.0410 | +1.0840 | 0.4588 | 431 / 219 / 374 |
| EMA | 33.1533 | +1.1963 | 0.4434 | 433 / 239 / 352 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 836/1024 | 839/1024 |
| Act 2 boss cleared | 344/1024 | 360/1024 |
| Act 3 first boss cleared | 110/1024 | 107/1024 |
| Act 3 second boss cleared | 45/1024 | 41/1024 |
| All three keys obtained | 242/1024 | 249/1024 |
| Act 4 entered | 44/1024 | 41/1024 |
| Shield and Spear defeated | 41/1024 | 34/1024 |
| Heart defeated | 6/1024 | 3/1024 |
| Ruby key | 254/1024 | 261/1024 |
| Emerald key | 381/1024 | 397/1024 |
| Sapphire key | 291/1024 | 296/1024 |
| Stalled | 0/1024 | 2/1024 |

EMA minus raw was +0.1123 floors with a paired SE of 0.4392 over 1,024 seeds. The independent CPU audit checked both sets of seed shards and ledgers, verified the baseline against the 76-hour audit, and reconciled training decisions across the two jobs.

The archived audit SHA256 is `425362692f24302eb71c836e09123b140fca635629d5e46108a51a50239f5a67`.
