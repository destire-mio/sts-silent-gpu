# E4 384 current-reward control at 34 training hours

Job 5468 completed update 1045 at 122443.982131443 training seconds and **273,940,461 cumulative training decisions**. Job 5468 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-34.pt` | 31.812500000 | -0.144531250 | 0.420254980 | 364 / 284 / 376 | 270/1,024 (26.37%) |
| EMA (.998) | `dev-34-ema.pt` | 33.376953125 | +1.419921875 | 0.428040890 | 427 / 274 / 323 | 317/1,024 (30.96%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 1045. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `b4cab4dc0caead2539877a5d2e832f9c088b115175b6d3e6b347ac10794545f8`
