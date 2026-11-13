# E4 evaluation at 56 training hours

Job 5605 resumed from update 1702, serial 177, and Adam step 27247. It completed both evaluations at update 1711, 201715.5539252092 training seconds and 448,528,315 cumulative training decisions. The running source was `e4220ea103486de80745e06c1e9ebcdc03956fa7`. The #203 recipe used entropy 0.005, strategic share 0.35, late-pool fraction 0.2 and key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use the per-seed floor differences. The 1,024-seed denominators include 0 stalled raw episodes and 3 stalled EMA episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.4727 | +0.5156 | 0.4442 | 400 / 251 / 373 |
| EMA | 32.8018 | +0.8447 | 0.4366 | 419 / 248 / 357 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 863/1024 | 858/1024 |
| Act 2 boss cleared | 316/1024 | 333/1024 |
| Act 3 first boss cleared | 76/1024 | 88/1024 |
| Act 3 second boss cleared | 21/1024 | 23/1024 |
| All three keys obtained | 195/1024 | 212/1024 |
| Act 4 entered | 21/1024 | 23/1024 |
| Shield and Spear defeated | 16/1024 | 20/1024 |
| Heart defeated | 1/1024 | 1/1024 |
| Ruby key | 205/1024 | 222/1024 |
| Emerald key | 354/1024 | 359/1024 |
| Sapphire key | 244/1024 | 249/1024 |
| Stalled | 0/1024 | 3/1024 |

EMA minus raw was +0.3291 floors with a paired SE of 0.3937 over 1,024 seeds. The independent CPU audit matched the raw and EMA seed shards, ledgers and summaries and recorded both snapshot hashes. It reconciled cumulative decisions from the audited 54-hour checkpoint through job 5604 updates 1652–1702 and job 5605 updates 1703–1711.

The archived audit SHA256 is `96adc0868502d74c88ce38e249fef469da7dd509a1f1ef459a1c94f2cc5165e6`.
