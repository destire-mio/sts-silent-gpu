# E4 384 current-reward control at 40 training hours

Job 5494 completed update 1228 at 144022.210683868 training seconds and **321,912,808 cumulative training decisions**. Job 5494 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-40.pt` | 31.436523438 | -0.520507812 | 0.427438470 | 376 / 250 / 398 | 278/1,024 (27.15%) |
| EMA (.998) | `dev-40-ema.pt` | 32.570312500 | +0.613281250 | 0.445892975 | 409 / 257 / 358 | 298/1,024 (29.10%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 1228. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `2135e048cd420c692f8866500e3adbe620af2d819f0389bbd506587c028b2ebe`
