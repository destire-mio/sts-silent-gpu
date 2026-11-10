# E4 384 current-reward control at 28 training hours

Job 5465 completed update 862 at 100836.319053239 training seconds and **225,968,112 cumulative training decisions**. Job 5465 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-28.pt` | 32.538085938 | +0.581054688 | 0.434534787 | 389 / 281 / 354 | 307/1,024 (29.98%) |
| EMA (.998) | `dev-28-ema.pt` | 32.341796875 | +0.384765625 | 0.440360363 | 382 / 280 / 362 | 311/1,024 (30.37%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 862. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `ec980874c270a9eeaa3910e86a7b3fefc854ed5504ed875b4b60280689413ab0`
