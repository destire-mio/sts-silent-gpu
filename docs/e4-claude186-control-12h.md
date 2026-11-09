# E4 384 current-reward control at 12 training hours

Job 5454 completed update 374 at 43274.719169328 training seconds and **98,041,854 cumulative training decisions**. Job 5454 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-12.pt` | 31.376953125 | -0.580078125 | 0.432376694 | 267/1,024 (26.07%) |
| EMA (.998) | `dev-12-ema.pt` | 32.044921875 | +0.087890625 | 0.417204057 | 273/1,024 (26.66%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 374. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `ab25cde2b8dd421389b5183f060863e25facdce825f32faeff2f2bac4da7600c`
