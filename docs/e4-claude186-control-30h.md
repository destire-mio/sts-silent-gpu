# E4 384 current-reward control at 30 training hours

Job 5466 completed update 923 at 108037.754303794 training seconds and **241,958,895 cumulative training decisions**. Job 5466 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors. Each change is the mean of the floor differences on matching seeds. The paired SE uses those same differences.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Seeds higher / tied / lower | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Raw | `dev-30.pt` | 32.797851562 | +0.840820312 | 0.424436941 | 404 / 271 / 349 | 315/1,024 (30.76%) |
| EMA (.998) | `dev-30-ema.pt` | 32.694335938 | +0.737304688 | 0.427425925 | 386 / 281 / 357 | 306/1,024 (29.88%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 923. The server coordination archive retains the original audit and the SHA-256 manifest for its 15 inputs.

Audit SHA-256 `fb8f94b0670632440d0d70cc353346e17e5e6a09487dd9b684a68ecf5bb5a905`
