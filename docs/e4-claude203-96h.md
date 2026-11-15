# E4 evaluation at 96 training hours

Job 5741 reached natural checkpoint 2897 at 345634.4392263179 training seconds and 759,431,051 cumulative training decisions. Job 5741 completed both the raw and EMA evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.2676 | +0.3105 | 0.4608 | 405 / 227 / 392 |
| EMA | 33.1279 | +1.1709 | 0.4590 | 432 / 233 / 359 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 838/1024 | 846/1024 |
| Act 2 boss cleared | 315/1024 | 345/1024 |
| Act 3 first boss cleared | 96/1024 | 103/1024 |
| Act 3 second boss cleared | 34/1024 | 39/1024 |
| All three keys obtained | 222/1024 | 248/1024 |
| Act 4 entered | 34/1024 | 39/1024 |
| Shield and Spear defeated | 31/1024 | 31/1024 |
| Heart defeated | 5/1024 | 2/1024 |
| Ruby key | 229/1024 | 259/1024 |
| Emerald key | 357/1024 | 389/1024 |
| Sapphire key | 266/1024 | 297/1024 |
| Stalled | 2/1024 | 1/1024 |

EMA minus raw was +0.8604 floors with paired SE 0.4186. The independent CPU audit matched both seed sets and ledgers, verified the server baseline against the 78-hour audit and reconciled cumulative training decisions. The archived audit SHA256 is `fb7bf1bc5b5011b2f071e08b3ee8d69a8d10a0b3d4c0653aaf3df0d88e0d82db`.

## Mainline status for #223

Training continued through the SSH monitoring outage. At 2026-11-15T06:05:50+00:00, Slurm showed 5743 running on two V100s and 5744 queued after it. Job 5743 resumed from natural checkpoint 2920, serial 310, with matched ranks and Adam step 46724. The latest completed update was 2928, at 349382.07816523756 training seconds and 767,557,515 decisions. The next ordinary evaluation is due at 352,800 training seconds (98 hours).
