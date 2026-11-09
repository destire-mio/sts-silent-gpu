# E4 384 current-reward control at 8 training hours

Job 5444 completed update 251 at 28873.800424692 training seconds and **65,798,142 cumulative training decisions**. Job 5444 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-8.pt` | 31.272460938 | -0.684570312 | 0.412191629 | 256/1,024 (25.00%) |
| EMA (.998) | `dev-8-ema.pt` | 32.187500000 | +0.230468750 | 0.401823069 | 287/1,024 (28.03%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 251. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `fd48438ace0def44d179d38fb60b65103815382dd009d855c6dbb2d852b9ba31`
