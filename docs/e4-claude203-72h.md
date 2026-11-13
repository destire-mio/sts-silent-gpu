# E4 evaluation at 72 training hours

Job 5651 evaluated raw and EMA weights at natural checkpoint 2187, 259243.42815743713 training seconds and 573,308,831 cumulative training decisions. Job 5641 ran updates 2128–2157. Job 5651 resumed from update 2157, serial 226, preserving EMA and Adam step 34525, including 3 saved partial optimizer steps. It ran updates 2158–2187. Both jobs used #203 with entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use per-seed floor differences. Every denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.1807 | +1.2236 | 0.4603 | 445 / 206 / 373 |
| EMA | 33.9971 | +2.0400 | 0.4333 | 468 / 232 / 324 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 838/1024 | 879/1024 |
| Act 2 boss cleared | 369/1024 | 373/1024 |
| Act 3 first boss cleared | 109/1024 | 105/1024 |
| Act 3 second boss cleared | 31/1024 | 37/1024 |
| All three keys obtained | 251/1024 | 252/1024 |
| Act 4 entered | 31/1024 | 37/1024 |
| Shield and Spear defeated | 21/1024 | 27/1024 |
| Heart defeated | 1/1024 | 0/1024 |
| Ruby key | 256/1024 | 263/1024 |
| Emerald key | 391/1024 | 384/1024 |
| Sapphire key | 306/1024 | 299/1024 |
| Stalled | 1/1024 | 2/1024 |

EMA minus raw was +0.8164 floors with a paired SE of 0.4300 over 1,024 seeds. The independent CPU audit checked both sets of seed shards and ledgers, verified the baseline against the 70-hour audit, and reconciled training decisions across the two jobs.

The archived audit SHA256 is `fda0fa7251d181cf2efa6cca719b3057e4e704214d4ea0340b88fc9100b49994`.
