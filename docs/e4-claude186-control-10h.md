# E4 384 current-reward control at 10 training hours

Job 5445 completed update 312 at 36048.655293705 training seconds and **81,788,926 cumulative training decisions**. Job 5448 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-10.pt` | 31.492187500 | -0.464843750 | 0.414700964 | 254/1,024 (24.80%) |
| EMA (.998) | `dev-10-ema.pt` | 32.255859375 | +0.298828125 | 0.401558032 | 278/1,024 (27.15%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 312. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `9b5f194762e9ca21f477c5c8e8e7658a27f6393afe4e52b7cf765de8c09ea38e`
