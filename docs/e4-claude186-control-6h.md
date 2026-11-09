# E4 384 current-reward control at 6 training hours

Job 5438 completed update 189 at 21674.889142181 training seconds and **49,545,214 cumulative training decisions**. Job 5438 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-6.pt` | 31.303710938 | -0.653320312 | 0.386228532 | 244/1,024 (23.83%) |
| EMA (.998) | `dev-6-ema.pt` | 31.363281250 | -0.593750000 | 0.400446840 | 247/1,024 (24.12%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 189. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `40390fdbb728b1d58bc465f95da0d2a382479ae4ca1496ee85114b486c8705fc`
