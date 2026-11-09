# E4 384 current-reward control at two training hours

Job 5436 evaluated update 63 at **16,515,072 cumulative training decisions** and 7205.379409 training seconds. It resumed the same control directory and source `d575609a06ad88418c79bfaf8e99928aec966e02` from job 5435. Both argmax evaluations completed all 1,024 seeds.

The shared `dev-112.pt` baseline averaged 31.957031 floors at zero training decisions. Changes and standard errors in the table are paired by seed against that baseline.

| Weights | Snapshot | Seeds | Mean floor | Change vs. dev-112 | Paired SE | Act 3 victories | Heart wins |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Raw | dev-2.pt | 1,024 | 30.972656 | -0.984375 | 0.426224 | 18 | 0 |
| EMA (.998) | dev-2-ema.pt | 1,024 | 32.641602 | +0.684570 | 0.365767 | 22 | 0 |

On the same seeds, EMA averaged **+1.668945 floors** relative to raw weights (paired SE 0.407741). EMA finished higher on 392 seeds, tied on 344, and finished lower on 288.

The CPU audit reconciled the original seed shards, the merged ledgers, and all 63 logged training updates. It recorded SHA-256 digests for both snapshots and all 15 inputs. The next paired evaluation is due at 14,400 training seconds.
