# E4 384 current-reward control at 16 training hours

Job 5457 completed update 497 at 57717.981729140 training seconds and **130,285,561 cumulative training decisions**. Job 5457 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-16.pt` | 32.195312500 | +0.238281250 | 0.423201286 | 287/1,024 (28.03%) |
| EMA (.998) | `dev-16-ema.pt` | 32.543945312 | +0.586914062 | 0.414283566 | 294/1,024 (28.71%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 497. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `68437a7b3e6c89f97ab893492d7a5c9ef488512f2d927c16b9799a99e232a9a1`
