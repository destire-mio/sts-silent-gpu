# E4 evaluation at 94 training hours

Job 5740 reached natural checkpoint 2838 at 338407.4501972995 training seconds and 743,964,558 cumulative training decisions. Job 5740 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 31.3828 | -0.5742 | 0.4755 | 408 / 211 / 405 |
| EMA | 32.1660 | +0.2090 | 0.4802 | 424 / 218 / 382 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 806/1024 | 831/1024 |
| Act 2 boss cleared | 302/1024 | 329/1024 |
| Act 3 first boss cleared | 86/1024 | 97/1024 |
| Act 3 second boss cleared | 35/1024 | 36/1024 |
| All three keys obtained | 199/1024 | 219/1024 |
| Act 4 entered | 35/1024 | 36/1024 |
| Shield and Spear defeated | 29/1024 | 26/1024 |
| Heart defeated | 3/1024 | 1/1024 |
| Ruby key | 224/1024 | 235/1024 |
| Emerald key | 352/1024 | 360/1024 |
| Sapphire key | 241/1024 | 277/1024 |
| Stalled | 6/1024 | 4/1024 |

EMA minus raw was +0.7832 floors with paired SE 0.4105. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `bb7698f65408bb4347513d58c3d40702847e6b4fc96b696f8ef053703b0f5973`.
