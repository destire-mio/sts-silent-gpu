# E4 evaluation at 110 training hours

Job 5761 reached natural checkpoint 3304 at 396118.6171484855 training seconds and 866,123,630 cumulative training decisions. Job 5761 completed both evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. All 1,024 episodes contribute to each denominator.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 31.7568 | -0.2002 | 0.4346 | 379 / 236 / 409 |
| EMA | 32.9814 | +1.0244 | 0.4338 | 421 / 263 / 340 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 835/1024 | 872/1024 |
| Act 2 boss cleared | 300/1024 | 321/1024 |
| Act 3 first boss cleared | 102/1024 | 89/1024 |
| Act 3 second boss cleared | 31/1024 | 29/1024 |
| All three keys obtained | 212/1024 | 219/1024 |
| Act 4 entered | 31/1024 | 29/1024 |
| Shield and Spear defeated | 29/1024 | 24/1024 |
| Heart defeated | 0/1024 | 3/1024 |
| Ruby key | 233/1024 | 241/1024 |
| Emerald key | 331/1024 | 357/1024 |
| Sapphire key | 245/1024 | 250/1024 |
| Stalled | 0/1024 | 4/1024 |

EMA minus raw was +1.2246 floors with paired SE 0.4229. The independent CPU audit matched the original two-rank seed shards and ledgers, checked the baseline against the preceding audit and reconciled training decisions through update 3304.

The archived audit SHA256 is `5541c8f9e8b441d5cc1509488d990f52af689c6307c94844e3857a5a2aea951e`.
