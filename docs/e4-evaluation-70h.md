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
