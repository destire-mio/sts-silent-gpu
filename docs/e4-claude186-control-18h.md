# E4 384 current-reward control at 18 training hours

Job 5459 completed update 557 at 64881.319755161 training seconds and **146,014,198 cumulative training decisions**. Job 5459 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-18.pt` | 32.280273438 | +0.323242188 | 0.427700177 | 293/1,024 (28.61%) |
| EMA (.998) | `dev-18-ema.pt` | 32.871093750 | +0.914062500 | 0.439305594 | 312/1,024 (30.47%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 557. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `d6b6ddfca49a84629ae76f2a34944afcc44ad2ff5b8d2eaae3ff238e6ca1ab1a`
