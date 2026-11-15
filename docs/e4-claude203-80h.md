# E4 evaluation at 80 training hours

Job 5679 reached natural checkpoint 2425 at 288049.3275637005 training seconds and 635,699,098 cumulative training decisions. Job 5679 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.5400 | +1.5830 | 0.4472 | 456 / 206 / 362 |
| EMA | 33.9355 | +1.9785 | 0.4557 | 459 / 222 / 343 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 852/1024 | 873/1024 |
| Act 2 boss cleared | 384/1024 | 385/1024 |
| Act 3 first boss cleared | 105/1024 | 115/1024 |
| Act 3 second boss cleared | 34/1024 | 39/1024 |
| All three keys obtained | 259/1024 | 265/1024 |
| Act 4 entered | 33/1024 | 39/1024 |
| Shield and Spear defeated | 27/1024 | 33/1024 |
| Heart defeated | 4/1024 | 4/1024 |
| Ruby key | 266/1024 | 269/1024 |
| Emerald key | 427/1024 | 395/1024 |
| Sapphire key | 340/1024 | 332/1024 |
| Stalled | 2/1024 | 0/1024 |

EMA minus raw was +0.3955 floors with paired SE 0.4380. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `4567e1888e480bf1df9bd581df365a3afc03376a7179e15b7b86879042a7f87b`.
