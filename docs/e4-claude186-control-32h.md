# E4 384 current-reward control at 32 training hours

Job 5467 completed update 984 at 115239.462840340 training seconds and **257,949,678 cumulative training decisions**. Job 5467 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-32.pt` | 32.401367188 | +0.444335938 | 0.427964381 | 407 / 246 / 371 | 305/1,024 (29.79%) |
| EMA (.998) | `dev-32-ema.pt` | 33.341796875 | +1.384765625 | 0.433891876 | 435 / 266 / 323 | 331/1,024 (32.32%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 984. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `51f10933522d03c072e7111ca76d60a2b9b90845d95b1f866adf434472c10c22`
