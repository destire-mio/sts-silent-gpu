# E4 384 current-reward control at 44 training hours

Original-recipe recovery trial 5548 completed update 1350 at 158475.407290434 training seconds and **353,894,372 cumulative training decisions**. It ran source `d575609a06ad88418c79bfaf8e99928aec966e02` with key-last-chance off.

Raw and EMA argmax used the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change and its paired SE use the floor differences on matching seeds.

| Weights | Snapshot | Mean floor | Paired change vs. server dev-112 | Paired SE | Seeds higher / tied / lower |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-44.pt` | 31.555664062 | -0.401367188 | 0.443623402 | 366 / 279 / 379 |
| EMA (.998) | `dev-44-ema.pt` | 32.479492188 | +0.522460938 | 0.450759018 | 408 / 256 / 360 |

Each stage count uses all 1,024 evaluated seeds as its denominator.

| Stage | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 824/1,024 | 829/1,024 |
| Act 2 boss cleared | 286/1,024 | 321/1,024 |
| Act 3 first boss cleared | 89/1,024 | 116/1,024 |
| Act 3 victory | 23/1,024 | 30/1,024 |
| All three keys obtained | 50/1,024 | 56/1,024 |
| Act 4 entered | 7/1,024 | 10/1,024 |
| Shield and Spear defeated | 5/1,024 | 8/1,024 |
| Heart defeated | 1/1,024 | 1/1,024 |

The independent CPU audit reconciled the original seed shards with both evaluation ledgers and checked the cumulative decision total against the training logs through update 1350. The coordination archive retains the original audit and SHA-256 manifest for all 15 inputs.

Audit SHA-256 `447ee85d73c30669823868ac1685845d499037bc6048f51668305cf1c24ce704`
