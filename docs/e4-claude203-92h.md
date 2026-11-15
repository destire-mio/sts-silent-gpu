# E4 evaluation at 92 training hours

Job 5739 reached natural checkpoint 2779 at 331254.7782094921 training seconds and 728,498,064 cumulative training decisions. Job 5739 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 30.8721 | -1.0850 | 0.4819 | 378 / 214 / 432 |
| EMA | 32.1982 | +0.2412 | 0.4789 | 423 / 216 / 385 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 806/1024 | 829/1024 |
| Act 2 boss cleared | 281/1024 | 332/1024 |
| Act 3 first boss cleared | 86/1024 | 110/1024 |
| Act 3 second boss cleared | 27/1024 | 41/1024 |
| All three keys obtained | 189/1024 | 232/1024 |
| Act 4 entered | 27/1024 | 40/1024 |
| Shield and Spear defeated | 25/1024 | 36/1024 |
| Heart defeated | 3/1024 | 3/1024 |
| Ruby key | 204/1024 | 245/1024 |
| Emerald key | 334/1024 | 362/1024 |
| Sapphire key | 223/1024 | 268/1024 |
| Stalled | 2/1024 | 0/1024 |

EMA minus raw was +1.3262 floors with paired SE 0.4507. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `836ccf299ff3b80beefc4533411b4166f905ef2ecc109b35b4cd0251b5caecd2`.
