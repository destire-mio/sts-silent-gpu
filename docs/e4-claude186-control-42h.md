# E4 384 current-reward control at 42 training hours

Job 5495 completed update 1289 at 151223.904821968 training seconds and **337,903,589 cumulative training decisions**. Job 5495 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-42.pt` | 31.719726562 | -0.237304688 | 0.440688587 | 383 / 262 / 379 | 306/1,024 (29.88%) |
| EMA (.998) | `dev-42-ema.pt` | 32.630859375 | +0.673828125 | 0.443547027 | 396 / 265 / 363 | 319/1,024 (31.15%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 1289. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `9f23e5d6b445c8a1922b19931dd498eac94b04f074fcbd77ea15b660b5bc5776`
