# E4 evaluation at 58 training hours

Job 5615 resumed from update 1765, serial 184, and Adam step 28253. It completed both evaluations at update 1770, 208909.50746814697 training seconds and 463,994,811 cumulative training decisions. The running source was `e4220ea103486de80745e06c1e9ebcdc03956fa7`. The #203 recipe used entropy 0.005, strategic share 0.35, late-pool fraction 0.2 and key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use the per-seed floor differences. The 1,024-seed denominators include one stalled raw episode and three stalled EMA episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.7666 | +0.8096 | 0.4662 | 424 / 232 / 368 |
| EMA | 33.4766 | +1.5195 | 0.4548 | 452 / 219 / 353 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 841/1024 | 861/1024 |
| Act 2 boss cleared | 348/1024 | 363/1024 |
| Act 3 first boss cleared | 108/1024 | 108/1024 |
| Act 3 second boss cleared | 30/1024 | 28/1024 |
| All three keys obtained | 237/1024 | 245/1024 |
| Act 4 entered | 30/1024 | 28/1024 |
| Shield and Spear defeated | 23/1024 | 25/1024 |
| Heart defeated | 2/1024 | 1/1024 |
| Ruby key | 245/1024 | 259/1024 |
| Emerald key | 397/1024 | 389/1024 |
| Sapphire key | 283/1024 | 299/1024 |
| Stalled | 1/1024 | 3/1024 |

EMA minus raw was +0.7100 floors with a paired SE of 0.4227 over 1,024 seeds. The independent CPU audit matched the raw and EMA seed shards, ledgers and summaries and recorded both snapshot hashes. It reconciled cumulative decisions from the audited 56-hour checkpoint through job 5606 updates 1712–1765 and job 5615 updates 1766–1770.

The archived audit SHA256 is `b35f0985bfc2c5e136a6c04d789fcca39536dc988252ea4378cd74dc863040ad`.
