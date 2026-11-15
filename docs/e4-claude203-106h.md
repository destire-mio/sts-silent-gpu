# E4 evaluation at 106 training hours

Job 5752 reached natural checkpoint 3188 at 381688.7535478575 training seconds and 835,714,934 cumulative training decisions. Job 5752 completed both evaluations. The #203 recipe used entropy 0.005 and source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`. Both evaluations ran with key-last-chance enabled.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired SE uses per-seed floor differences. All 1,024 episodes contribute to each denominator.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 31.8525 | -0.1045 | 0.4593 | 409 / 225 / 390 |
| EMA | 32.8633 | +0.9063 | 0.4585 | 429 / 237 / 358 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 825/1024 | 841/1024 |
| Act 2 boss cleared | 318/1024 | 346/1024 |
| Act 3 first boss cleared | 81/1024 | 107/1024 |
| Act 3 second boss cleared | 25/1024 | 42/1024 |
| All three keys obtained | 206/1024 | 246/1024 |
| Act 4 entered | 25/1024 | 42/1024 |
| Shield and Spear defeated | 20/1024 | 34/1024 |
| Heart defeated | 3/1024 | 3/1024 |
| Ruby key | 211/1024 | 256/1024 |
| Emerald key | 355/1024 | 371/1024 |
| Sapphire key | 245/1024 | 284/1024 |
| Stalled | 2/1024 | 3/1024 |

EMA minus raw was +1.0107 floors with paired SE 0.4157. The independent CPU audit matched the original two-rank seed shards and ledgers, checked the baseline against the preceding audit and reconciled training decisions through update 3188.
The worker log records `engine_fault:852769260` on EMA seed 2,000,000,731, which is included in that mode's stalled count.
The worker log records `engine_fault:852769260` on EMA seed 2,000,000,749, which is included in that mode's stalled count.

The archived audit SHA256 is `30f4f65e4f34d8f5adcd3309c068736f3f67be3d6db5e74cea2a4ba943462905`.
