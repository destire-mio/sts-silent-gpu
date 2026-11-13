# E4 startup failure after the 60-hour evaluation

Job 5617 completed normally and saved update 1868, serial 195, at 220867.84561298264 training seconds and 489,684,922 decisions. The ordinary 60-hour raw and EMA evaluations had already completed and been audited at update 1828.

Jobs 5628, 5631, 5632 and 5633 each loaded update 1868 on two V100s. The saved `dev-60.pt` triggered the legacy #137 lookahead supplement at startup. Both ranks hit its eight-rank assertion before another training update could complete. Slurm recorded FAILED 1:0 for all four jobs. Their worker logs contain the assertion.

| Job | Slurm result | Elapsed seconds | Charge (node-s) |
| --- | --- | ---: | ---: |
| 5617 | COMPLETED 0:0 | 6963 | 1740.75 |
| 5628 | FAILED 1:0 | 302 | 75.50 |
| 5631 | FAILED 1:0 | 297 | 74.25 |
| 5632 | FAILED 1:0 | 297 | 74.25 |
| 5633 | FAILED 1:0 | 297 | 74.25 |

The original controller settled each terminal record once. The settled ledger is 429480.50 node-s. The successor guard was set at 08:56:32 UTC. Jobs 5632 and 5633 were already in the chain.

The [lookahead fix](https://github.com/destire-mio/sts-silent-gpu/commit/4a856d13f9f8c446ccd05c8b05e3c27fdbed3a69) keeps the legacy supplement on eight-rank allocations and skips it on smaller allocations. Both new one- and two-rank cases reproduced the assertion with the old code. Five native CPU cases passed with the fix. The verified rollback launcher uses the original recipe and starts with the entropy coefficient at 0.01.

The original controller submitted job 5634 at its calculated 4744 MiB, with two V100s and nine CPUs. Both ranks loaded the natural update 1868 checkpoint with Adam step 29902, including two saved partial optimizer steps. Update 1869 completed at 2152.0 decisions/s with 16 optimizer steps, finite metrics, zero AMP skips and 21 episode faults. The legacy supplement did not start. The successor guard stays on during the recovery check. The ordinary 62-hour pair remains due at 223200 training seconds.
