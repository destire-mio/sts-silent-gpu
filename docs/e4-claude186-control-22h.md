# E4 384 current-reward control at 22 training hours

Job 5461 completed update 679 at 79276.281556363 training seconds and **177,995,762 cumulative training decisions**. Job 5461 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-22.pt` | 31.547851562 | -0.409179688 | 0.456167215 | 295/1,024 (28.81%) |
| EMA (.998) | `dev-22-ema.pt` | 32.165039062 | +0.208007812 | 0.420098315 | 293/1,024 (28.61%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 679. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `cf72f361ec31d0395659ca97fda31522e29f20f94fc9c500c7ab633a038a88f5`
