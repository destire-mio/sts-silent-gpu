# E4 evaluation at 90 training hours

Job 5738 reached natural checkpoint 2720 at 324116.10905616847 training seconds and 713,031,570 cumulative training decisions. Job 5738 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.9434 | +0.9863 | 0.4830 | 433 / 201 / 390 |
| EMA | 33.0146 | +1.0576 | 0.4710 | 435 / 201 / 388 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 836/1024 | 826/1024 |
| Act 2 boss cleared | 354/1024 | 360/1024 |
| Act 3 first boss cleared | 114/1024 | 121/1024 |
| Act 3 second boss cleared | 38/1024 | 49/1024 |
| All three keys obtained | 254/1024 | 255/1024 |
| Act 4 entered | 37/1024 | 49/1024 |
| Shield and Spear defeated | 31/1024 | 43/1024 |
| Heart defeated | 4/1024 | 4/1024 |
| Ruby key | 270/1024 | 269/1024 |
| Emerald key | 384/1024 | 389/1024 |
| Sapphire key | 304/1024 | 304/1024 |
| Stalled | 1/1024 | 3/1024 |

EMA minus raw was +0.0713 floors with paired SE 0.4268. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `c5a480d7a86435217ec18cfd66019156738941dbb6f74de5b4cea7e534d909df`.
