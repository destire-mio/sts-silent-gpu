# E4 evaluation at 50 training hours

Job 5601 resumed from update 1489, serial 156, and Adam step 23837, with EMA retained. It completed both evaluations at update 1531, 180009.67759479687 training seconds and 401,342,396 cumulative training decisions. The running source was `e4220ea103486de80745e06c1e9ebcdc03956fa7`. The #203 recipe used entropy 0.005, strategic share 0.35, late-pool fraction 0.2 and key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Each paired standard error uses the per-seed floor differences. The one stalled EMA episode is included in all 1,024-seed denominators.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 30.8682 | -1.0889 | 0.4398 | 358 / 245 / 421 |
| EMA | 32.0303 | +0.0732 | 0.4366 | 378 / 269 / 377 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 836/1024 | 852/1024 |
| Act 2 boss cleared | 276/1024 | 296/1024 |
| Act 3 first boss cleared | 67/1024 | 78/1024 |
| Act 3 second boss cleared | 16/1024 | 19/1024 |
| All three keys obtained | 160/1024 | 201/1024 |
| Act 4 entered | 16/1024 | 19/1024 |
| Shield and Spear defeated | 13/1024 | 16/1024 |
| Heart defeated | 3/1024 | 0/1024 |
| Ruby key | 171/1024 | 206/1024 |
| Emerald key | 297/1024 | 348/1024 |
| Sapphire key | 206/1024 | 235/1024 |
| Stalled | 0/1024 | 1/1024 |

EMA minus raw was +1.1621 floors with a paired SE of 0.4127 over 1,024 seeds. The independent CPU audit matched the raw and EMA seed shards, ledgers and summaries. It reconciled cumulative decisions from the audited 48-hour checkpoint through job 5600 updates 1472–1489 and job 5601 updates 1490–1531.

The archived audit SHA256 is `953dbb13633ac8696230c03df05609abdf6ef314c3476e1d14b5a1ed34402a56`.
