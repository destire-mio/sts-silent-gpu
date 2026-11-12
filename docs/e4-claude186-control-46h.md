# E4 384 control evaluation at 46 training hours

Job 5559 saved update 1410, serial 146, at 165629.23454487184 training seconds and 369,623,011 cumulative training decisions. Job 5562 loaded the matching checkpoint and Adam step 22568. It ran both 1,024-seed evaluations with the original runtime `d575609a06ad88418c79bfaf8e99928aec966e02` and key-last-chance disabled.

The paired baseline was the server's `dev-112.pt` on the same 1,024 seeds, with a mean of 31.9570 floors. The standard errors use the per-seed floor differences.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 31.8721 | -0.0850 | 0.4728 | 426 / 214 / 384 |
| EMA | 32.5596 | +0.6025 | 0.4447 | 405 / 269 / 350 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 820/1024 | 830/1024 |
| Act 2 boss cleared | 300/1024 | 323/1024 |
| Act 3 first boss cleared | 119/1024 | 111/1024 |
| Act 3 second boss cleared | 37/1024 | 27/1024 |
| All three keys obtained | 41/1024 | 67/1024 |
| Act 4 entered | 11/1024 | 11/1024 |
| Shield and Spear defeated | 10/1024 | 9/1024 |
| Heart defeated | 0/1024 | 1/1024 |
| Ruby key | 147/1024 | 132/1024 |
| Emerald key | 227/1024 | 221/1024 |
| Sapphire key | 84/1024 | 125/1024 |
| Stalled | 2/1024 | 1/1024 |

EMA minus raw was +0.6875 floors with a paired SE of 0.4412 across all 1,024 seeds. The independent CPU audit matched the seed shards, ledgers, summaries, paired differences and cumulative decisions from the previously audited 44-hour checkpoint. The 46-hour audit SHA256 is `1b0cd13b3ce3e86678fa22aafcd9d936b6609e2ed15e191c1aa6808a929d27d0`.
