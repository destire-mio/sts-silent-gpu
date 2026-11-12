# E4 evaluation at 52 training hours

Job 5602 resumed from update 1543, serial 161, and Adam step 24701. It completed both evaluations at update 1591, 187227.21827543434 training seconds and 417,071,036 cumulative training decisions. The running source was `e4220ea103486de80745e06c1e9ebcdc03956fa7`. The #203 recipe used entropy 0.005, strategic share 0.35, late-pool fraction 0.2 and key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Each paired standard error uses the per-seed floor differences. The denominators include one stalled raw episode and two stalled EMA episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.5195 | +0.5625 | 0.4215 | 415 / 241 / 368 |
| EMA | 32.6992 | +0.7422 | 0.4278 | 403 / 261 / 360 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 860/1024 | 863/1024 |
| Act 2 boss cleared | 320/1024 | 318/1024 |
| Act 3 first boss cleared | 75/1024 | 69/1024 |
| Act 3 second boss cleared | 24/1024 | 20/1024 |
| All three keys obtained | 209/1024 | 209/1024 |
| Act 4 entered | 24/1024 | 20/1024 |
| Shield and Spear defeated | 20/1024 | 16/1024 |
| Heart defeated | 0/1024 | 6/1024 |
| Ruby key | 211/1024 | 211/1024 |
| Emerald key | 350/1024 | 342/1024 |
| Sapphire key | 248/1024 | 258/1024 |
| Stalled | 1/1024 | 2/1024 |

EMA minus raw was +0.1797 floors with a paired SE of 0.4112 over 1,024 seeds. The independent CPU audit matched the raw and EMA seed shards, ledgers and summaries. It reconciled cumulative decisions from the audited 50-hour checkpoint through job 5601 updates 1532–1543 and job 5602 updates 1544–1591.

The archived audit SHA256 is `162d5218d5beba4f4b1715cbdfc49a883015cc25cc24a76719f4479cdd3df8e5`.
