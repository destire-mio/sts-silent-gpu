# E4 384 current-reward control at 20 training hours

Job 5460 completed update 617 at 72010.564606084 training seconds and **161,742,836 cumulative training decisions**. Job 5460 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-20.pt` | 32.411132812 | +0.454101562 | 0.458340806 | 315/1,024 (30.76%) |
| EMA (.998) | `dev-20-ema.pt` | 32.210937500 | +0.253906250 | 0.452205786 | 303/1,024 (29.59%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 617. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `3c8a8839b6b7b114ddde6770a4582d3f5bbcb8bdbdeb433f6bb9c3bfa668d693`
