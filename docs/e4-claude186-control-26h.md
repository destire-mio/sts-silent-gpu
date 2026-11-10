# E4 384 current-reward control at 26 training hours

Job 5463 completed update 801 at 93660.268974535 training seconds and **209,977,328 cumulative training decisions**. Job 5463 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-26.pt` | 32.156250000 | +0.199218750 | 0.437324709 | 406 / 257 / 361 | 291/1,024 (28.42%) |
| EMA (.998) | `dev-26-ema.pt` | 32.523437500 | +0.566406250 | 0.423060987 | 399 / 286 / 339 | 301/1,024 (29.39%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 801. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `564b37b3266fda87e7d7e082f87f177c665c2a02291eb40fa284fd86453fdd28`
