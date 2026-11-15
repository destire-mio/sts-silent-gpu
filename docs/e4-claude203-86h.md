# E4 evaluation at 86 training hours

Job 5726 reached natural checkpoint 2602 at 309711.06038700533 training seconds and 682,098,582 cumulative training decisions. Job 5726 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.0137 | +1.0566 | 0.4674 | 438 / 221 / 365 |
| EMA | 34.1240 | +2.1670 | 0.4488 | 460 / 230 / 334 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 834/1024 | 852/1024 |
| Act 2 boss cleared | 348/1024 | 396/1024 |
| Act 3 first boss cleared | 102/1024 | 110/1024 |
| Act 3 second boss cleared | 37/1024 | 43/1024 |
| All three keys obtained | 244/1024 | 275/1024 |
| Act 4 entered | 37/1024 | 43/1024 |
| Shield and Spear defeated | 35/1024 | 39/1024 |
| Heart defeated | 5/1024 | 4/1024 |
| Ruby key | 255/1024 | 290/1024 |
| Emerald key | 375/1024 | 418/1024 |
| Sapphire key | 295/1024 | 328/1024 |
| Stalled | 1/1024 | 5/1024 |

EMA minus raw was +1.1104 floors with paired SE 0.4614. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `ef5fbbcf8b7fea527f4e64ae9b3f32001847be9272bcf9f38bc996e954f5e504`.
