# E4 evaluation at 74 training hours

Job 5676 evaluated raw and EMA weights at natural checkpoint 2247, 266441.47136359685 training seconds and 589,037,470 cumulative training decisions. Job 5675 ran updates 2188–2234. Job 5676 resumed from update 2234, serial 234, preserving EMA and Adam step 35758, including 5 saved partial optimizer steps. It ran updates 2235–2247. Both jobs used #203 with entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use per-seed floor differences. Every denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.0723 | +1.1152 | 0.4567 | 431 / 224 / 369 |
| EMA | 33.3057 | +1.3486 | 0.4563 | 449 / 222 / 353 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 849/1024 | 849/1024 |
| Act 2 boss cleared | 354/1024 | 353/1024 |
| Act 3 first boss cleared | 91/1024 | 102/1024 |
| Act 3 second boss cleared | 35/1024 | 28/1024 |
| All three keys obtained | 239/1024 | 246/1024 |
| Act 4 entered | 35/1024 | 28/1024 |
| Shield and Spear defeated | 30/1024 | 24/1024 |
| Heart defeated | 0/1024 | 3/1024 |
| Ruby key | 248/1024 | 262/1024 |
| Emerald key | 375/1024 | 394/1024 |
| Sapphire key | 292/1024 | 297/1024 |
| Stalled | 0/1024 | 1/1024 |

EMA minus raw was +0.2334 floors with a paired SE of 0.4441 over 1,024 seeds. The independent CPU audit checked both sets of seed shards and ledgers, verified the baseline against the 72-hour audit, and reconciled training decisions across the two jobs.

The archived audit SHA256 is `f5ff434e371a9609372a780d77732660d873f6810bdbc242762e2677daba7844`.
