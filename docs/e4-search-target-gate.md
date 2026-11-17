# E4 combat search target stability gate

The formal run used the frozen u3363 EMA with key-last-chance enabled and the existing `CombatLookahead` from `056fd13a`. It played 1,024 fresh source games by policy argmax across the seed intervals `[1920000000, 1920000512)` and `[1921000000, 1921000512)`, with one interval per V100. Uniform reservoir sampling selected up to two natural battle decisions per game across Acts 1–4.

Each sampled decision ran four independent searches, with up to eight candidate actions and four determinizations per action under the existing search guards. A stable override required at least three searches to choose the same non-argmax action. Pairwise agreement uses all six comparisons at every point where at least one search changed the policy action.

Each of the 56 stable overrides and its policy action shared 16 fresh validation futures, for 896 complete pairs. Both arms followed the policy argmax through the end of that fight. Remaining HP is zero on death and is the actual HP on escape. The win indicator counts battle wins. The analysis averages the paired differences within each decision, then estimates standard errors clustered by source game. The lower 95% bound is the mean minus 1.96 standard errors.

## Overall

| Source games | Sampled / valid decisions | Any change | Pairwise agreement | Stable overrides | Fully paired overrides | Remaining HP diff. ± SE | HP 95% lower bound | Win diff. ± SE (pp) |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1,024 | 2,048 / 2,048 | 339 / 2,048 (16.55%) | 857 / 2,034 (42.13%) | 56 / 2,048 (2.73%) | 56 | +1.60 ± 0.84 | -0.042 | +2.01 ± 2.73 |

## By act and room

| Act | Room | Source games | Valid decisions | Any change | Pairwise agreement | Stable overrides | Fully paired overrides | Remaining HP diff. ± SE | Win diff. ± SE (pp) |
| ---: | :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | Hallway | 464 | 580 | 57 | 158 / 342 (46.20%) | 15 / 580 (2.59%) | 15 | +3.75 ± 1.17 | -6.67 ± 6.67 |
| 1 | Elite | 190 | 216 | 33 | 96 / 198 (48.48%) | 9 / 216 (4.17%) | 9 | -1.16 ± 2.21 | +12.50 ± 11.02 |
| 1 | Boss | 226 | 244 | 44 | 99 / 264 (37.50%) | 3 / 244 (1.23%) | 3 | +1.23 ± 1.29 | 0.00 ± 0.00 |
| 2 | Hallway | 363 | 431 | 81 | 218 / 486 (44.86%) | 16 / 431 (3.71%) | 16 | +3.52 ± 1.14 | +2.73 ± 2.35 |
| 2 | Elite | 128 | 136 | 19 | 44 / 114 (38.60%) | 1 / 136 (0.74%) | 1 | +7.38 (SE —) | 0.00 (SE —) |
| 2 | Boss | 144 | 153 | 36 | 88 / 216 (40.74%) | 3 / 153 (1.96%) | 3 | -6.94 ± 8.06 | +10.42 ± 10.42 |
| 3 | Hallway | 120 | 135 | 27 | 60 / 162 (37.04%) | 4 / 135 (2.96%) | 4 | +0.70 ± 0.50 | 0.00 ± 0.00 |
| 3 | Elite | 82 | 88 | 27 | 65 / 162 (40.12%) | 4 / 88 (4.55%) | 4 | -1.45 ± 5.27 | 0.00 ± 0.00 |
| 3 | Boss | 54 | 60 | 14 | 27 / 84 (32.14%) | 1 / 60 (1.67%) | 1 | +0.44 (SE —) | +25.00 (SE —) |
| 4 | Hallway | 0 | 0 | 0 | — | 0 | 0 | — | — |
| 4 | Elite | 3 | 3 | 1 | 2 / 6 (33.33%) | 0 / 3 (0.00%) | 0 | — | — |
| 4 | Boss | 2 | 2 | 0 | — | 0 / 2 (0.00%) | 0 | — | — |

A dash indicates an empty denominator or unavailable estimate. A single paired source game gives a mean with no clustered standard error.

## Throughput and provenance

The single-V100 pilot (Slurm 5787) produced one fully paired override in 69 allocation seconds, or 52.17 per hour. The formal run (Slurm 5788) produced 56 in 1,461 allocation seconds on two V100s, or **137.99 fully paired overrides per allocation hour** (68.99 per GPU-hour).

The two formal journals retain 64 + 64 immutable chunks with decision paths, chosen actions, search and validation seeds, and paired outcomes. The terminal audit SHA256 is `d1df0104c184a41f5fc154acf4d2411308e325ad24b1724c3da41f34c1ca5358` and the independent raw-chunk cross-check SHA256 is `0c259ebae13bb7b8c510a2a49847e7051361d03abbcfea9d284dff040d9d8852`. The frozen EMA SHA256 is `b81d04a0e0f3df7d235f90e2090f703886f24def0678e37d8a14530fb944d054`, and the engine SHA256 is `ab8c2156c026027ddd3f30edf3a4dbde355342ec126b28774f925bb763ccf3c3`.

**Gate failed.** Pairwise agreement was 42.13% against the 70% minimum. The lower 95% bound on remaining HP difference was -0.042 HP against the required positive bound.
