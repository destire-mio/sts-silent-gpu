# E4 384 current-reward control at 4 training hours

Job 5437 completed update 126 at 14429.823386042 training seconds and **33,030,143 cumulative training decisions**. Job 5437 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-4.pt` | 31.483398438 | -0.473632812 | 0.410455859 | 260/1,024 (25.39%) |
| EMA (.998) | `dev-4-ema.pt` | 31.567382812 | -0.389648438 | 0.386877528 | 266/1,024 (25.98%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 126. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `d4eff032d3c115a2b97573051d269541f482bc29441719771c8f1fccb9080e68`
