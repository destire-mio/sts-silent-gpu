# E4 384 current-reward control at 38 training hours

Job 5493 completed update 1167 at 136823.785173871 training seconds and **305,922,027 cumulative training decisions**. Job 5493 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-38.pt` | 31.467773438 | -0.489257812 | 0.417411985 | 363 / 282 / 379 | 265/1,024 (25.88%) |
| EMA (.998) | `dev-38-ema.pt` | 32.830078125 | +0.873046875 | 0.427632643 | 388 / 289 / 347 | 312/1,024 (30.47%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 1167. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `0a07ab9e522a20d92b8f6af01beded9fe41891f1ea65d22059408707dd4f50cb`
