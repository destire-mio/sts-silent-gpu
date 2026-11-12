# Claude #203 server mainline startup

Slurm job 5559 hot-resumed the final natural checkpoint of 5548 at update 1366, serial 143, with 160369.02275171364 training seconds and 358,088,675 decisions. Both ranks matched the checkpoint. Adam resumed at step 21,864 and EMA carried forward with decay .998. The original control used entropy coefficient .01.

The canonical batch launched `c214-claude203-preparation/launch-a1cc` with published source `a1cc559c15248b34b89c078b8dacdf9a96e4ebbd` and the `7c75b76e` recipe arguments. The worker recorded strategic share .35, entropy coefficient .005, late-pool fraction .2 with the E2 holdout excluded, and key-last-chance enabled. `dev-40-ema.pt` remains available at its original path.

| Update | Seconds | Global dec/s | CUDA allocated GiB by rank | CUDA reserved GiB by rank | Entropy | KL | Clip fraction | Value loss | Late starts | Strategic weight share |
| ---: | ---: | ---: | :--- | :--- | ---: | ---: | ---: | ---: | :--- | :--- |
| 1367 | 116.37 | 2252.8 | 5.320 / 4.853 | 12.354 / 13.154 | 0.511542 | 0.006920 | 0.052351 | 0.00044778 | 114/608 (18.75%) | 10.69% → 35.00% |
| 1368 | 117.94 | 2222.7 | 5.445 / 5.180 | 10.447 / 12.846 | 0.513107 | 0.006731 | 0.050184 | 0.00047068 | 135/684 (19.74%) | 10.48% → 35.00% |
| 1369 | 120.07 | 2183.2 | 5.242 / 5.572 | 10.447 / 12.951 | 0.514281 | 0.006898 | 0.048887 | 0.00043895 | 118/662 (17.82%) | 10.14% → 35.00% |

The GPU columns list rank 0 then rank 1. The highest reservation was 13.154 GiB across the two assigned Tesla V100-SXM2-16GB cards. The job cgroup recorded a host-memory peak of 5022728192 bytes against a 7975469056-byte limit, with zero OOM events. Each update completed 16 optimizer steps with zero AMP skips. The three updates recorded zero episode faults in 14,226 cases. The late-start denominator counts actual episode starts. The strategic column shows the unweighted share and the share computed from the applied advantage weights.

The first three updates passed the startup check through 160723.40379357163 training seconds and 358,875,107 decisions. The unchanged controller re-enabled normal scheduling and submitted 5560 as the sole successor with `afterany:5559`. Slurm verified 5560 pending on that dependency. The controller settled 5548 once at 1739.25 node-s using its completed 6957-second allocation on two cards. The cumulative ledger was 410540.75 node-s.

The next ordinary 1024-seed raw and EMA pair is due at 165600 training seconds (46 h).
