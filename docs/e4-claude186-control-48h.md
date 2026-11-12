# E4 384 control evaluation at 48 training hours

Job 5600 resumed from update 1465, serial 151, and Adam step 23448. It completed both evaluations at update 1471, 172,878.41036711982 training seconds and 385,613,757 cumulative training decisions. The original runtime was `d575609a06ad88418c79bfaf8e99928aec966e02`, with entropy 0.01 and key-last-chance disabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired differences and their standard errors use each seed's floor difference.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 31.2021 | -0.7549 | 0.4456 | 385 / 238 / 401 |
| EMA | 32.3164 | +0.3594 | 0.4607 | 401 / 245 / 378 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 793/1024 | 839/1024 |
| Act 2 boss cleared | 285/1024 | 307/1024 |
| Act 3 first boss cleared | 91/1024 | 110/1024 |
| Act 3 second boss cleared | 24/1024 | 28/1024 |
| All three keys obtained | 54/1024 | 67/1024 |
| Act 4 entered | 10/1024 | 13/1024 |
| Shield and Spear defeated | 8/1024 | 12/1024 |
| Heart defeated | 2/1024 | 2/1024 |
| Ruby key | 86/1024 | 135/1024 |
| Emerald key | 250/1024 | 235/1024 |
| Sapphire key | 131/1024 | 117/1024 |
| Stalled | 0/1024 | 2/1024 |

EMA minus raw was +1.1143 floors with a paired SE of 0.4443 across all 1,024 seeds. The independent CPU audit matched both sets of seed shards, ledgers and summaries and reconciled the training decisions from the audited 46-hour checkpoint. The 48-hour audit SHA256 is `6e5183938994783c1f427ee31c1984a13d414d29a67e80d322ad0c2cc22202ac`.
