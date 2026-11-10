# E4 384 current-reward control at 36 training hours

Job 5475 completed update 1106 at 129660.220024577 training seconds and **289,931,244 cumulative training decisions**. Job 5475 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-36.pt` | 32.035156250 | +0.078125000 | 0.440144993 | 388 / 265 / 371 | 296/1,024 (28.91%) |
| EMA (.998) | `dev-36-ema.pt` | 32.612304688 | +0.655273438 | 0.414236381 | 411 / 266 / 347 | 305/1,024 (29.79%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 1106. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `9b8bae9f51c3bb429ee9bcd6bc2f826a7ffade05ab225ec05c0b2aea6b27c8ac`
