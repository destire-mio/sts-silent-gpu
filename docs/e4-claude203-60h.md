# E4 evaluation at 60 training hours

Job 5617 resumed from update 1816, serial 190, and Adam step 29075. It completed both evaluations at update 1828, 216013.8355591502 training seconds and 479,199,162 cumulative training decisions. The running source was `e4220ea103486de80745e06c1e9ebcdc03956fa7`. The #203 recipe used entropy 0.005, strategic share 0.35, late-pool fraction 0.2 and key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use the per-seed floor differences. The 1,024-seed denominators include 3 stalled raw episodes and 2 stalled EMA episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.2061 | +0.2490 | 0.4366 | 413 / 226 / 385 |
| EMA | 33.8291 | +1.8721 | 0.4380 | 445 / 235 / 344 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 844/1024 | 865/1024 |
| Act 2 boss cleared | 327/1024 | 392/1024 |
| Act 3 first boss cleared | 82/1024 | 103/1024 |
| Act 3 second boss cleared | 19/1024 | 25/1024 |
| All three keys obtained | 207/1024 | 260/1024 |
| Act 4 entered | 19/1024 | 25/1024 |
| Shield and Spear defeated | 15/1024 | 18/1024 |
| Heart defeated | 0/1024 | 0/1024 |
| Ruby key | 214/1024 | 269/1024 |
| Emerald key | 350/1024 | 407/1024 |
| Sapphire key | 282/1024 | 324/1024 |
| Stalled | 3/1024 | 2/1024 |

EMA minus raw was +1.6230 floors with a paired SE of 0.4295 over 1,024 seeds. The independent CPU audit matched the raw and EMA seed shards, ledgers and summaries and recorded both snapshot hashes. It reconciled cumulative decisions from the audited 58-hour checkpoint through job 5615 updates 1771–1816 and job 5617 updates 1817–1828.

The archived audit SHA256 is `349c135e325960a5522121bc3b1ae333c2bd8a67cb2cd0d3b1ac46a3aeb1881d`.
