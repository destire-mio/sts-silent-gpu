# E4 evaluation at 54 training hours

Job 5604 resumed from update 1650, serial 171, and Adam step 26415. It completed both evaluations at update 1651, 194458.82848857692 training seconds and 432,799,675 cumulative training decisions. The running source was `e4220ea103486de80745e06c1e9ebcdc03956fa7`. The #203 recipe used entropy 0.005, strategic share 0.35, late-pool fraction 0.2 and key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Each paired standard error uses the per-seed floor differences. Each weight set had two stalled episodes, included in the 1,024-seed denominators.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 31.5322 | -0.4248 | 0.4397 | 375 / 246 / 403 |
| EMA | 33.0898 | +1.1328 | 0.4391 | 413 / 252 / 359 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 833/1024 | 851/1024 |
| Act 2 boss cleared | 292/1024 | 344/1024 |
| Act 3 first boss cleared | 86/1024 | 97/1024 |
| Act 3 second boss cleared | 26/1024 | 30/1024 |
| All three keys obtained | 202/1024 | 223/1024 |
| Act 4 entered | 25/1024 | 30/1024 |
| Shield and Spear defeated | 19/1024 | 22/1024 |
| Heart defeated | 0/1024 | 5/1024 |
| Ruby key | 208/1024 | 233/1024 |
| Emerald key | 354/1024 | 377/1024 |
| Sapphire key | 234/1024 | 276/1024 |
| Stalled | 2/1024 | 2/1024 |

EMA minus raw was +1.5576 floors with a paired SE of 0.4363 over 1,024 seeds. The independent CPU audit matched the raw and EMA seed shards, ledgers and summaries. It reconciled cumulative decisions from the audited 52-hour checkpoint through job 5602 updates 1592–1595, job 5603 updates 1596–1650 and job 5604 update 1651.

The archived audit SHA256 is `cf041c9e37bd8d5fd6bf173478afdb9f370c93c6cf45ae2b63a61bbf955306ec`.
