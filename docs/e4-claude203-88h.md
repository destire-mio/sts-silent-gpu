# E4 evaluation at 88 training hours

Job 5727 reached natural checkpoint 2661 at 316893.4083168579 training seconds and 697,565,077 cumulative training decisions. Job 5727 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.4561 | +0.4990 | 0.4567 | 411 / 218 / 395 |
| EMA | 33.6006 | +1.6436 | 0.4719 | 446 / 231 / 347 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 838/1024 | 855/1024 |
| Act 2 boss cleared | 331/1024 | 379/1024 |
| Act 3 first boss cleared | 102/1024 | 115/1024 |
| Act 3 second boss cleared | 30/1024 | 33/1024 |
| All three keys obtained | 223/1024 | 271/1024 |
| Act 4 entered | 30/1024 | 32/1024 |
| Shield and Spear defeated | 26/1024 | 25/1024 |
| Heart defeated | 3/1024 | 3/1024 |
| Ruby key | 237/1024 | 282/1024 |
| Emerald key | 358/1024 | 407/1024 |
| Sapphire key | 273/1024 | 324/1024 |
| Stalled | 4/1024 | 1/1024 |

EMA minus raw was +1.1445 floors with paired SE 0.4611. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `20df1124c09844ac0a00ff9ebf155a56ed5a7b77e6223fa84d148a58bba389b2`.
