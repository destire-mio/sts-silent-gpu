# E4 evaluation at 62 training hours

Job 5634 completed the raw and EMA evaluations at natural checkpoint 1888, 223269.01514566562 training seconds and 494,927,780 cumulative training decisions. It resumed the #203 checkpoint at update 1868, serial 195, with Adam step 29902. Updates 1869–1888 used the #186 recipe, entropy 0.01 and source `d575609a06ad88418c79bfaf8e99928aec966e02` with the lookahead startup fix. Both evaluations ran with key-last-chance disabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. The paired standard errors use the per-seed floor differences. Each denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 33.3457 | +1.3887 | 0.4545 | 443 / 231 / 350 |
| EMA | 34.1553 | +2.1982 | 0.4391 | 468 / 240 / 316 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 842/1024 | 865/1024 |
| Act 2 boss cleared | 361/1024 | 379/1024 |
| Act 3 first boss cleared | 141/1024 | 140/1024 |
| Act 3 second boss cleared | 38/1024 | 41/1024 |
| All three keys obtained | 34/1024 | 42/1024 |
| Act 4 entered | 9/1024 | 9/1024 |
| Shield and Spear defeated | 7/1024 | 6/1024 |
| Heart defeated | 2/1024 | 2/1024 |
| Ruby key | 98/1024 | 107/1024 |
| Emerald key | 228/1024 | 269/1024 |
| Sapphire key | 222/1024 | 233/1024 |
| Stalled | 2/1024 | 0/1024 |

EMA minus raw was +0.8096 floors with a paired SE of 0.4411 over 1,024 seeds. The independent CPU audit matched both sets of seed shards and ledgers, recorded the snapshot hashes, and reconciled the cumulative decision count from the 60-hour audit through 5617 updates 1829–1868 and 5634 updates 1869–1888.

The archived audit SHA256 is `b5a217f74bca517ae00b8bce0b1540d49ca17ce425d8987eade7cb4f88de898e`.
