# E4 evaluation at 108 training hours

Job 5757 reached natural checkpoint 3245 at 388821.57883977075 training seconds and 850,657,140 cumulative training decisions. Job 5757 completed both evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. All 1,024 episodes contribute to each denominator.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 31.4209 | -0.5361 | 0.4401 | 369 / 247 / 408 |
| EMA | 32.0889 | +0.1318 | 0.4466 | 421 / 226 / 377 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 837/1024 | 852/1024 |
| Act 2 boss cleared | 299/1024 | 308/1024 |
| Act 3 first boss cleared | 72/1024 | 94/1024 |
| Act 3 second boss cleared | 22/1024 | 28/1024 |
| All three keys obtained | 193/1024 | 204/1024 |
| Act 4 entered | 21/1024 | 28/1024 |
| Shield and Spear defeated | 12/1024 | 23/1024 |
| Heart defeated | 4/1024 | 3/1024 |
| Ruby key | 200/1024 | 216/1024 |
| Emerald key | 322/1024 | 343/1024 |
| Sapphire key | 231/1024 | 247/1024 |
| Stalled | 0/1024 | 3/1024 |

EMA minus raw was +0.6680 floors with paired SE 0.4035. The independent CPU audit matched the original two-rank seed shards and ledgers, checked the baseline against the preceding audit and reconciled training decisions through update 3245.
The worker log records `engine_fault:852769260` on EMA seed 2,000,000,749, which is included in that mode's stalled count.

The archived audit SHA256 is `32e5b97f97f510cf4d823911b2faadeb5e71905765f5a2e70926deed204344bd`.
