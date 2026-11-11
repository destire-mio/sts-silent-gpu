# Claude #203 fixed E2 stage exam

The server CPU exam completed on **108 fixed roots**, four shared determinized futures per root and **432 completed futures per model**. It compared the server `dev-112.pt` against the preserved `dev-40-ema.pt`. The root cap was 32 per stage. Play ended at the current combat boundary; the numbers are conditional on these starting states.

The standard errors use root means across the four futures. Paired differences are EMA minus dev-112 on matching root and determinization seeds.

| Stage | Paired roots | dev-112 wins, rate ± SE | dev-40 EMA wins, rate ± SE | Paired change ± SE (pp) |
|---|---:|---:|---:|---:|
| Act 2 boss | 32 | 15/128 (11.72% ± 4.20 pp) | 18/128 (14.06% ± 3.71 pp) | +2.34 ± 3.78 |
| Act 3 first boss | 32 | 4/128 (3.13% ± 1.86 pp) | 6/128 (4.69% ± 3.45 pp) | +1.56 ± 2.49 |
| Act 3 second boss | 0 | unavailable | unavailable | unavailable |
| Shield/Spear | 12 | 8/48 (16.67% ± 8.88 pp) | 12/48 (25.00% ± 11.10 pp) | +8.33 ± 4.70 |
| Heart | 32 | 0/128 (0.00% ± 0.00 pp) | 0/128 (0.00% ± 0.00 pp) | 0.00 ± 0.00 |

| Stage | dev-112 mean HP lost | EMA mean HP lost | Paired HP change ± SE | dev-112 mean potions | EMA mean potions |
|---|---:|---:|---:|---:|---:|
| Act 2 boss | 47.68 | 48.40 | +0.72 ± 0.92 | 0.77 | 0.73 |
| Act 3 first boss | 29.83 | 29.47 | -0.36 ± 0.37 | 0.94 | 0.91 |
| Shield/Spear | 29.17 | 29.63 | +0.46 ± 0.79 | 0.48 | 0.54 |
| Heart | 14.91 | 14.91 | 0.00 ± 0.00 | 0.23 | 0.24 |

**Act 3 second boss is unavailable in this released E2 root set.** These paths use boss discovery profile 0 and reach Act 4 after one Act 3 boss. A recorded heart prefix, seed 1000002317, confirms a floor-50 boss followed by Act 4 without a second boss. Its denominator is zero; it is not a zero win rate. In contrast, both models actually lost all 128 Heart futures. A profile-511 held-out second-boss root set is still needed for that group.

Wall clock on one CPU thread: dev-112 **470.828 s**, EMA **632.412 s**, full run including fixed-root construction **1229.080 s**. This was independent process 1112708, not a training allocation.

The input E2 accepted replay SHA256 is `1e0633a0d99eb70e0789173c8f0596b407e6fe4f6360a689cb1087f6211d9162`. Both source prefixes and exact derived combat prefixes pass the 20% root hash holdout. The original files include the fixed root set, 14 candidate shards and 14 baseline shards, checkpoint hashes and original report. A separate standard-library CPU audit reproduced all rates, HP and potion means, all paired SEs, and every matching draw seed. Audit SHA256: `a01056f4f5aaf93ceb839763c226081c20ff967a64673f960d7eaabdb957dd06`.

The new recipe's first three production updates remain unmeasured because the server mainline is still in the native health stop after the startup-memory failure. This stage exam is independent of the paused production run.
