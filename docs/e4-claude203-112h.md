# E4 evaluation at 112 training hours

Job 5762 reached natural checkpoint 3363 at 403294.99822232407 training seconds and 881,590,125 cumulative training decisions. Job 5762 completed both evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. All 1,024 episodes contribute to each denominator.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 31.7549 | -0.2021 | 0.4676 | 397 / 221 / 406 |
| EMA | 32.8457 | +0.8887 | 0.4473 | 429 / 238 / 357 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 828/1024 | 850/1024 |
| Act 2 boss cleared | 297/1024 | 332/1024 |
| Act 3 first boss cleared | 92/1024 | 95/1024 |
| Act 3 second boss cleared | 26/1024 | 32/1024 |
| All three keys obtained | 211/1024 | 223/1024 |
| Act 4 entered | 26/1024 | 32/1024 |
| Shield and Spear defeated | 21/1024 | 28/1024 |
| Heart defeated | 3/1024 | 1/1024 |
| Ruby key | 221/1024 | 237/1024 |
| Emerald key | 357/1024 | 362/1024 |
| Sapphire key | 240/1024 | 263/1024 |
| Stalled | 2/1024 | 1/1024 |

EMA minus raw was +1.0908 floors with paired SE 0.4338. The independent CPU audit matched the original two-rank seed shards and ledgers, checked the baseline against the preceding audit and reconciled training decisions through update 3363.

The archived audit SHA256 is `77fb1cb0bf30002f23d866fda358a744ebbf98fa12f5437b0311d32a98695485`.
