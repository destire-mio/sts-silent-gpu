# E4 384 current-reward control at 24 training hours

Job 5462 completed update 740 at 86457.173939334 training seconds and **193,986,544 cumulative training decisions**. Job 5462 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-24.pt` | 32.125000000 | +0.167968750 | 0.441376648 | 381 / 277 / 366 | 293/1,024 (28.61%) |
| EMA (.998) | `dev-24-ema.pt` | 33.081054688 | +1.124023438 | 0.431558827 | 415 / 272 / 337 | 318/1,024 (31.05%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 740. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `a0e44fb34bf24494d3cc59128578c9b3bb9c7a0249a92477798e1a0fd56418e4`
