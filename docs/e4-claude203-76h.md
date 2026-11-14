# E4 evaluation at 76 training hours

Job 5677 evaluated raw and EMA weights at natural checkpoint 2307, 273694.2406216229 training seconds and 604,766,109 cumulative training decisions. Job 5676 ran updates 2248–2286. Job 5677 resumed from update 2286, serial 239, preserving EMA and Adam step 36594, including 9 saved partial optimizer steps. It ran updates 2287–2307. Both jobs used #203 with entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use per-seed floor differences. Every denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.9941 | +1.0371 | 0.4421 | 422 / 249 / 353 |
| EMA | 33.4473 | +1.4902 | 0.4460 | 442 / 240 / 342 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 851/1024 | 855/1024 |
| Act 2 boss cleared | 341/1024 | 361/1024 |
| Act 3 first boss cleared | 93/1024 | 115/1024 |
| Act 3 second boss cleared | 31/1024 | 28/1024 |
| All three keys obtained | 227/1024 | 259/1024 |
| Act 4 entered | 31/1024 | 28/1024 |
| Shield and Spear defeated | 28/1024 | 25/1024 |
| Heart defeated | 3/1024 | 3/1024 |
| Ruby key | 239/1024 | 275/1024 |
| Emerald key | 380/1024 | 381/1024 |
| Sapphire key | 275/1024 | 302/1024 |
| Stalled | 1/1024 | 1/1024 |

EMA minus raw was +0.4531 floors with a paired SE of 0.4154 over 1,024 seeds. The independent CPU audit checked both sets of seed shards and ledgers, verified the baseline against the 74-hour audit, and reconciled training decisions across the two jobs.

The archived audit SHA256 is `69ac29fbea4c44512edc75f537f4cc18f469b6543850e3e6f8e7003dfaf903c1`.
