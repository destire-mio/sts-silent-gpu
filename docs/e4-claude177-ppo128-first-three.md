# E4 server PPO with a 128-step rollout

Job 5433 switched after update 74 at training clock 481569.8204895028 s. The runtime was `5f1b2c2a8b5e48c478fdbe4a3d9e3aca8103dbe0`. The two V100s each ran 1024 lanes for 128 steps, giving 262,144 decisions per round. PPO used a global minibatch of 32768 and two epochs.

## First three complete rounds

| Update | Sampling (s) | Learner (s) | Cross-rank peak allocated (bytes) | Cross-rank peak reserved (bytes) | Value MSE | Cumulative 128-step decisions |
|---:|---:|---:|---:|---:|---:|---:|
| 75 | 114.22293301299214 | 374.5490360358963 | 8531799552 | 12842958848 | 0.0005369254969307415 | 262,144 |
| 76 | 106.15940636198502 | 373.8617521219421 | 7912804352 | 12842958848 | 0.0005547536837081904 | 524,288 |
| 77 | 109.64414750994183 | 371.78418272163253 | 7801993216 | 8334082048 | 0.0006120653250781122 | 786,432 |

Each round applied all 16 optimizer attempts, with zero AMP skips and zero episode faults. Peak memory is the maximum over both ranks reported by the learner instrumentation.

For comparisons from dev-132, the server collected 3,145,728 decisions in the 64-step layout before the switch and 786,432 in the 128-step group through update 77. The total since dev-132 is 3,932,160. The 134-hour ordinary evaluation used update 76, at 524,288 cumulative 128-step decisions.

| Canonical log row | Bytes | SHA-256 |
|---|---:|---|
| Switch marker | 149 | `752d65005def274d64402fe38855265671e691eaf62a261061a81cd9e96713b0` |
| Update 75 | 2922 | `31ff0c232d72afb67aa361fa6737e39e7da30704d5be7f2e32a1b4ceb5704759` |
| Update 76 | 2935 | `0b5cd21766c690d62e77215378f5e4f6d51ce8530054dac36bed76414c87aa57` |
| Update 77 | 2931 | `e09e2de0365f0268c7d28e3a34026aa0b1c7e3ab6161de2f457d9b40ad88c30f` |
