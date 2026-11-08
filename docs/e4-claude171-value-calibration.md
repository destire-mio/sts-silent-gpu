# E4 #171 · 768 PPO value coefficient calibration

The 768 student began PPO after the dev-132 ordinary evaluation. Job 5432 measured the value-to-actor gradient ratio on the first PPO update. Job 5433 resumed the same measurement state and logged completion on round 20. Both jobs used two V100s.

- Measured g_value / g_actor: `0.0032483795044997185`.
- Inherited coefficient: `181.3911719048255`.
- Target coefficient: `76.96145098000254` (0.25 / ratio, clipped to [0.5, 200]).

| PPO round | Update | Job | Training seconds | Coefficient | Value MSE |
|---:|---:|---:|---:|---:|---:|
| 1 | 63 | 5432 | 476247.40247325954 | 181.3911719048255 | 0.00030730400631981446 |
| 10 | 72 | 5432 | 480610.08475068264 | 131.92446199306727 | 0.00037093938723510686 |
| 20 | 82 | 5433 | 485424.3765383368 | 76.96145098000254 | 0.0006111158163414123 |

Rounds 1 and 10 used the 64-step rollout. Round 20 used the #177 128-step rollout. The MSE entries are the logged learner losses for each round.

Canonical completion row SHA256: `5d908ea44ed91875b60bc666ca9f05cfd0fc402884da9066aae31b37e3f3fc2f`.
