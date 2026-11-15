# E4 evaluation at 100 training hours

Job 5744 reached natural checkpoint 3015 at 360020.885344496 training seconds and 790,364,041 cumulative training decisions. Job 5744 completed both evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. All 1,024 episodes contribute to each denominator.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.3770 | +0.4199 | 0.4521 | 418 / 228 / 378 |
| EMA | 33.1445 | +1.1875 | 0.4567 | 445 / 229 / 350 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 854/1024 | 866/1024 |
| Act 2 boss cleared | 325/1024 | 348/1024 |
| Act 3 first boss cleared | 95/1024 | 106/1024 |
| Act 3 second boss cleared | 34/1024 | 37/1024 |
| All three keys obtained | 217/1024 | 242/1024 |
| Act 4 entered | 34/1024 | 37/1024 |
| Shield and Spear defeated | 28/1024 | 32/1024 |
| Heart defeated | 4/1024 | 4/1024 |
| Ruby key | 236/1024 | 257/1024 |
| Emerald key | 353/1024 | 381/1024 |
| Sapphire key | 260/1024 | 283/1024 |
| Stalled | 3/1024 | 1/1024 |

EMA minus raw was +0.7676 floors with paired SE 0.4295. The independent CPU audit matched the original two-rank seed shards and ledgers, checked the baseline against the preceding audit and reconciled training decisions through update 3015.

The archived audit SHA256 is `6a11a53bd5993ca96f4cbdf5591cbe0844463c1fee6f97622473fe67e6311d36`.
