# E4 384 current-reward control at 14 training hours

Job 5456 completed update 436 at 50513.262997762 training seconds and **114,294,779 cumulative training decisions**. Job 5456 completed both argmax evaluations. The control source was `d575609a06ad88418c79bfaf8e99928aec966e02`.

Both weight sets completed the same 1,024 seeds. The server `dev-112.pt` baseline averaged 31.957031250 floors on those seeds. Changes and standard errors use the differences on matching seeds.

| Weights | Snapshot | Mean floor | Change vs. server dev-112 | Paired SE | Act 2 boss cleared |
| --- | --- | ---: | ---: | ---: | ---: |
| Raw | `dev-14.pt` | 31.890625000 | -0.066406250 | 0.412256896 | 271/1,024 (26.46%) |
| EMA (.998) | `dev-14-ema.pt` | 31.875000000 | -0.082031250 | 0.404278562 | 282/1,024 (27.54%) |

The CPU audit reconciled both sets of original seed shards and ledgers and summed the logged training decisions through update 436. The original audit and SHA-256 manifest for its 15 inputs are retained in the server coordination archive.

Audit SHA-256 `c04d2029592749f82915489c7d5de52dfa68c7f8b03734fb906aca3f38cc65e5`
