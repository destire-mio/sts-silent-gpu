# E4 evaluation at 70 training hours

The first ordinary evaluation using observation version **14** ran at update **1492** after **252078.499** accumulated training seconds. The policy selected actions by argmax in 1,024 games from the start, using the fixed seeds 2000000000–2000001023 split across eight GPU ranks. The counts below were recomputed from the eight evaluation shards.

| Metric | 68h (v13) | 70h (v14) |
| --- | ---: | ---: |
| Mean floor | 29.737305 | 29.349609 |
| Act 1 boss passed | 830 | 810 |
| Act 2 boss passed | 211 | 205 |
| First Act 3 boss passed | 42 | 44 |
| Act 3 wins | 9 | 11 |
| Heart wins | 0 | 0 |

The paired mean floor difference from 68h was **-0.387695** (SE 0.393960). The paired difference from the fixed teacher baseline was **+11.879883** (SE 0.387950). These comparisons cover this fixed seed set. The 68h and 70h checkpoints also differ in training time and observation version, so the paired change does not isolate the effect of the migration.

The 70h evaluation recorded 1012 deaths, 11 Act 3 wins and 1 stalled game. Ruby, emerald and sapphire key counts were 10, 166 and 11. No game collected all three keys or entered Act 4.

## Training window and verification

The interval `(244840.0398180486, 252078.49940035964]` contains 49 PPO updates. Updates 1444–1479 come from the previous allocation through its durable checkpoint. Updates 1480–1492 come from the resumed v14 run. Updates 1480–1486 and the unfinished 1487 from the interrupted allocation are excluded.

The window recorded 3135 optimizer steps and 1 AMP skip. Learning rate was `0.000013168724279835392` throughout. KL ranged from 0.005705 to 0.018508 and ended at 0.009099.

Student decision counts total 1,048,576: combat 792228, card reward 26640, map 50344, shop 15041, campfire 24264, and other 140059.

The Act 2 ledger matched the merged eight rank shards and contains 810 Act 2 entries. Tied-rank Spearman correlations between value and final floor were 0.553929 at the Act 1 boss (942 cases) and 0.554103 at Act 2 entry (810 cases). Each correlation is conditional on its recorded subset.

The retained weights-only `dev-70.pt` loaded strictly on CPU with 12,435,074 parameters and finite weights. Its SHA-256 is `d838ecc2796f11c884775d704ee88f9b6655f460d90b35b3f87ac9aca0a1d7f8` (49762853 bytes). The per-episode ledger SHA-256 is `e7a70208431a216b104554255248f7a838345320488b7a025619504756d062de` (1614235 bytes).

## Expert iteration after the v14 resume

These are the 13 v14 rounds through this evaluation. L is the lane count used in the round, and next L is the controller's logged request for the next round. The allocation gate can reduce that request before the next collection. All memory columns are in bytes. Peak and projection are the logged expert metrics, and per-lane bytes are the measured collection increment per lane. Events are grouped by their position between successive update records in the existing job log. A dash indicates no matching event in that interval.

| Update | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocated | Measured bytes/lane | Memory limit | OOM recovered |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 1480 | 1 | 2 | 101 | 6.48 | 64 | 15.12% | 0.6251 | 85.15% | 2693826048 | 4607726178 | 47534080 | — | — |
| 1481 | 2 | 4 | 151 | 19.21 | 64 | 7.52% | 0.8737 | 82.78% | 2910088704 | 5080423620 | 38516992 | — | — |
| 1482 | 4 | 2 | 357 | 14.90 | 53 | 22.87% | 0.8198 | 80.95% | 2795587072 | 5565578632 | 26475008 | — | — |
| 1483 | 2 | 4 | 189 | 9.61 | 64 | 18.09% | 0.8152 | 79.89% | 3105961984 | 5328634052 | 25717504 | — | — |
| 1484 | 4 | 8 | 324 | 13.25 | 35 | 21.44% | 0.8191 | 78.40% | 3013451264 | 5813789064 | 20500992 | — | — |
| 1485 | 8 | 16 | 480 | 19.59 | 29 | 23.40% | 0.8308 | 78.33% | 3030902784 | 6689731856 | 18420864 | — | — |
| 1486 | 16 | 32 | 641 | 25.31 | 21 | 24.97% | 0.9964 | 80.50% | 2998976000 | 8629511200 | 10778528 | — | — |
| 1487 | 32 | 64 | 1038 | 40.95 | 12 | 25.41% | 0.8435 | 78.61% | 3023574016 | 12511190592 | 11658160 | — | — |
| 1488 | 34 | 34 | 1170 | 45.72 | 16 | 26.11% | 0.7695 | 81.62% | 2976484864 | 12996345604 | 10937329 | Yes, 64 → 34 | — |
| 1489 | 33 | 33 | 1232 | 46.75 | 16 | 22.21% | 0.8469 | 80.19% | 3498687488 | 13451535522 | 12012715 | Yes, 34 → 33 | — |
| 1490 | 33 | 33 | 1287 | 48.55 | 19 | 22.35% | 0.8892 | 80.03% | 3677024768 | 13539176098 | 14641184 | — | — |
| 1491 | 33 | 33 | 1412 | 53.04 | 21 | 22.75% | 0.8482 | 79.53% | 3527685120 | 13539176098 | 10916321 | — | — |
| 1492 | 33 | 33 | 1434 | 55.00 | 14 | 22.56% | 0.7693 | 79.43% | 3768239104 | 13708322466 | 12922787 | — | — |

The memory gate reduced the requested 64 lanes to 34 at update 1488, then 34 to 33 at update 1489. No expert OOM recovery event was recorded in these 13 rounds. The final round used 33 lanes and collected 55.00 expert samples per second with 22.56% overhead.
