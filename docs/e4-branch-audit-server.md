# E4 natural decision branch audit

The production run collected **256 natural decision points**, including **128** that were simulated through the next Act's Boss. It recorded 51,424 branch-and-future rows and 13,798,026 simulated decisions.

The two Acts each targeted 128 roots, including 64 extended roots. The frozen u3363 EMA played argmax after the tested choice using the existing engine. Each shard sampled at most one root of a given Act and decision type per game, using reservoir sampling over eligible natural decisions. The campfire sample used the final map row. Each usable campfire option and each eligible individual upgrade or removal was evaluated, including cancellation when available. The simulation journals retain the game seed, the path of actions to the root, legal branch metadata, shared future seeds and per-future outcomes.

Every branch at a root shared the same random seeds. Short estimates used the first 16 futures. The horizon comparison used 32 shared futures at each extended root. Death counts as a loss. There were 6 incomplete short rows and 88 incomplete long rows. Paired results use roots with complete outcomes for every compared branch. The retrospective best branch can benefit from selecting on the same futures. Ties prefer the original policy choice, then the first branch in native candidate order. SE is the sample standard deviation of the root-level estimates divided by the square root of the number of roots.

## Current-Act Boss clearance

| Act | Decision | Paired / sampled roots | Policy passes | Best branch passes | Best minus policy |
| --- | --- | --- | --- | --- | --- |
| 1 | Card reward | 44 / 44 | 88.35% | 92.19% | +3.84 ± 0.96 pp |
| 1 | Map | 42 / 42 | 86.90% | 90.48% | +3.57 ± 1.64 pp |
| 1 | Final campfire | 42 / 42 | 86.16% | 87.65% | +1.49 ± 0.56 pp |
| 2 | Card reward | 43 / 44 | 51.45% | 55.96% | +4.51 ± 1.03 pp |
| 2 | Map | 41 / 42 | 45.43% | 49.39% | +3.96 ± 1.02 pp |
| 2 | Final campfire | 42 / 42 | 61.16% | 64.43% | +3.27 ± 0.96 pp |

### Elite route choices

| Act | Paired map roots | Mean elite option minus policy | Best elite option minus policy |
| --- | --- | --- | --- |
| 1 | 42 | -10.57 ± 3.44 pp | -10.57 ± 3.44 pp |
| 2 | 41 | -4.78 ± 2.69 pp | -4.57 ± 2.69 pp |

Each root weights its legal elite branches equally for the mean-elite comparison. The best-elite column uses the highest observed elite-branch clearance rate at the same root.

### Taking the policy's card versus skipping

| Act | Deck size before reward | Paired card choices | Take minus skip | Paired roots with policy skip or other choice |
| --- | --- | --- | --- | --- |
| 1 | <=15 | 20 | +2.50 ± 2.50 pp | 4 |
| 1 | 16-20 | 17 | +4.04 ± 1.93 pp | 2 |
| 1 | 21-25 | 1 | +0.00 pp (SE unavailable) | 0 |
| 1 | 26-30 | 0 | unavailable | 0 |
| 1 | >30 | 0 | unavailable | 0 |
| 2 | <=15 | 0 | unavailable | 0 |
| 2 | 16-20 | 6 | -4.17 ± 4.75 pp | 3 |
| 2 | 21-25 | 17 | +8.09 ± 2.67 pp | 8 |
| 2 | 26-30 | 7 | +4.46 ± 5.74 pp | 2 |
| 2 | >30 | 0 | unavailable | 0 |

## Boss-entry HP

HP percentiles use trials that reached the current Act's Boss. Entry counts give the denominator for each row. The original policy and best-branch rows overlap the action-type rows.

| Act | Decision | Branch | Reached / trials | Q1 | Median | Q3 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Final campfire | best | 672 / 672 | 41.0 | 49.0 | 54.0 |
| 1 | Final campfire | dig | 16 / 16 | 66.0 | 66.0 | 66.0 |
| 1 | Final campfire | policy | 672 / 672 | 40.0 | 48.5 | 56.0 |
| 1 | Final campfire | recall | 672 / 672 | 30.0 | 42.5 | 50.0 |
| 1 | Final campfire | rest | 672 / 672 | 49.0 | 65.0 | 66.0 |
| 1 | Final campfire | smith | 11072 / 11072 | 30.0 | 42.0 | 50.0 |
| 1 | Final campfire | smith cancel | 672 / 672 | 49.0 | 65.0 | 66.0 |
| 1 | Final campfire | toke | 304 / 304 | 35.0 | 35.0 | 35.0 |
| 1 | Final campfire | toke cancel | 16 / 16 | 35.0 | 35.0 | 35.0 |
| 1 | Map | best | 666 / 672 | 44.0 | 53.0 | 62.0 |
| 1 | Map | elite | 615 / 672 | 40.0 | 49.0 | 59.0 |
| 1 | Map | nonelite | 785 / 816 | 45.0 | 53.0 | 62.0 |
| 1 | Map | policy | 650 / 672 | 44.0 | 53.0 | 62.0 |
| 2 | Final campfire | best | 672 / 672 | 43.0 | 60.0 | 66.0 |
| 2 | Final campfire | dig | 32 / 32 | 41.0 | 45.5 | 50.0 |
| 2 | Final campfire | policy | 672 / 672 | 43.0 | 60.5 | 66.0 |
| 2 | Final campfire | recall | 672 / 672 | 30.0 | 45.0 | 60.0 |
| 2 | Final campfire | rest | 624 / 624 | 51.0 | 66.0 | 71.0 |
| 2 | Final campfire | smith | 9984 / 9984 | 28.0 | 41.0 | 56.0 |
| 2 | Final campfire | smith cancel | 592 / 592 | 43.0 | 60.0 | 66.0 |
| 2 | Final campfire | toke | 768 / 768 | 49.0 | 49.0 | 60.0 |
| 2 | Final campfire | toke cancel | 32 / 32 | 60.0 | 63.0 | 66.0 |
| 2 | Map | best | 514 / 656 | 46.0 | 60.0 | 66.0 |
| 2 | Map | elite | 467 / 704 | 46.0 | 58.0 | 66.0 |
| 2 | Map | nonelite | 586 / 768 | 46.0 | 59.0 | 66.0 |
| 2 | Map | policy | 517 / 672 | 46.0 | 59.0 | 66.0 |

## Current-Act versus next-Act decisions

| Act | Decision | Extended roots | Short–long best agreement | Short–long Spearman | Forecast–long best agreement | Forecast–long Spearman | Forecast pair concordance |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | All types | 64 | 43.10% (58) | 0.311 (47) | 39.66% (58) | 0.275 (53) | 62.57% (53) |
| 1 | Card reward | 22 | 42.11% (19) | 0.284 (16) | 36.84% (19) | 0.254 (17) | 62.25% (17) |
| 1 | Map | 22 | 61.90% (21) | 0.396 (17) | 61.90% (21) | 0.333 (18) | 64.81% (18) |
| 1 | Final campfire | 20 | 22.22% (18) | 0.237 (14) | 16.67% (18) | 0.237 (18) | 60.61% (18) |
| 2 | All types | 64 | 43.86% (57) | 0.134 (39) | 54.39% (57) | 0.473 (44) | 74.32% (44) |
| 2 | Card reward | 22 | 57.14% (21) | 0.204 (16) | 47.62% (21) | 0.334 (17) | 68.82% (17) |
| 2 | Map | 22 | 40.00% (20) | -0.131 (12) | 75.00% (20) | 0.769 (14) | 88.57% (14) |
| 2 | Final campfire | 20 | 31.25% (16) | 0.321 (11) | 37.50% (16) | 0.336 (13) | 66.14% (13) |

Parentheses give roots contributing to each statistic. Spearman uses average ranks for tied branches and requires variation in both scores. Forecast concordance compares branch pairs with unequal actual long-horizon clearance rates and gives half credit when their forecasts tie.

The forecast score multiplies the current-Act Boss clearance rate by the mean next-Boss prediction among its clears. The frozen #227 evaluator reads the short-horizon card, relic, potion and HP snapshot, with the next encounter identified at the natural Act transition. There were 24,222 per-future predictions.

## Reproducibility

Sample quotas were met. Production seed intervals were `[1910000000, 1911000000)` and `[1911000000, 1912000000)`. The pilot used a separate seed interval. Both production intervals are disjoint from the #227 collection seeds.

The frozen EMA SHA256 is `b81d04a0e0f3df7d235f90e2090f703886f24def0678e37d8a14530fb944d054`. The deck evaluator SHA256 is `95a2bd8ac51b1242b098223dbe6dc6eab9c1c2959301ab9f5cf9707c57a25ff9`. The plan SHA256 is `81a5c226f5a927af77bbca346efafa046aeaafe3393ad5e650f02c6b9044a904`. The final independent data and accounting audit SHA256 is `35645116e38316c6f4a6740722428e87f97c17a31d475b0f41f4b8be1258b93c`.

The first allocation recorded 102 decision points. 3 card rewards appeared after the current Act's Boss had already been defeated. The audit excluded those rewards from both horizons and carried forward the other 99 points and all their recorded outcomes. The corrected collector resumed from the two saved seed positions. The original terminal audit SHA256 is `ea0ac04a5b877fed5c3c96145f39aae82c7463e28e0718b12c121d4640842e41`. The journal derivation SHA256 is `5d5ad2417ee0e5b1de9e165c189ccda7aee7b6dff74e6b6fe0002141ba61e941`.

The two pilot allocations consumed 90 seconds. Pilot and production allocations consumed 10,955 seconds of the 57,600-second cap. The table gives actual production allocation time and charged node-seconds.

| Slurm job | State | Exit | V100s | Seconds | Charged node-s |
| --- | --- | --- | --- | --- | --- |
| 5775 | COMPLETED | 0:0 | 2 | 6805 | 1701.25 |
| 5776 | COMPLETED | 0:0 | 2 | 4060 | 1015.0 |

## Held-out best branch

The first eight shared futures at each root selected the branch with the highest Boss clearance rate. Ties favored the policy choice, then native candidate order. The next eight shared futures scored that branch and the original policy choice. SE is computed across paired roots. Two Act 2 roots had incomplete selection futures.

| Act | Decision | Paired / sampled roots | Policy passes | Selected branch passes | Selected minus policy |
| --- | --- | --- | --- | --- | --- |
| 1 | Card reward | 44 / 44 | 89.20% | 88.92% | -0.28 ± 0.95 pp |
| 1 | Map | 42 / 42 | 85.12% | 86.61% | +1.49 ± 1.36 pp |
| 1 | Final campfire | 42 / 42 | 86.31% | 83.63% | -2.68 ± 1.24 pp |
| 2 | Card reward | 43 / 44 | 51.74% | 51.16% | -0.58 ± 1.90 pp |
| 2 | Map | 41 / 42 | 45.73% | 45.73% | +0.00 ± 1.31 pp |
| 2 | Final campfire | 42 / 42 | 62.80% | 60.71% | -2.08 ± 1.41 pp |
