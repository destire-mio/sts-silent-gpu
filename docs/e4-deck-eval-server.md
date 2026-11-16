# E4 server deck evaluator

On 1,024 matched seeds, the deck evaluator changed mean floor by **-6.8613 ± 0.4381** (paired standard error) relative to plain argmax using the same EMA.

## Checkpoint and collection

The run used the #203 EMA from update 3363 (403,294.998 training seconds and 881,590,125 decisions). The training checkpoint retains both ranks, Adam step 53,810 and the original recipe.

- Deck evaluator source `e618ffad60148fbb1fe6b64ee19546cfa27b2dce`. Frozen EMA SHA256 `b81d04a0e0f3df7d235f90e2090f703886f24def0678e37d8a14530fb944d054`.
- The one-V100 pilot (5767) collected 32 roots across the four Boss nodes in 167.679 worker seconds, or about **87.3 minutes per 1,000 roots**. Slurm recorded 174 seconds. Its 128 label rows were complete.
- Four production allocations used two V100s for **7:34:16** (27,256 elapsed allocation seconds). The two workers used disjoint seed ranges `[1900100000, 1901100000)` and `[1901100000, 1902100000)`.
- Each root had the original deck and three variants with 16 shared random draws. The audit found **9,400 roots** and **37,560/37,600 complete label rows**. The other 40 rows lack a complete win-rate label.

| Collection job | Slurm state | Elapsed seconds | Charged node-seconds |
| --- | --- | ---: | ---: |
| 5768 | COMPLETED | 6,815 | 1,703.75 |
| 5769 | COMPLETED | 6,812 | 1,703.00 |
| 5770 | COMPLETED | 6,828 | 1,707.00 |
| 5771 | COMPLETED | 6,801 | 1,700.25 |

## Held-out fit

The CPU fit used 5,000 updates, batch size 128, learning rate 0.0003 and random seed 130. Fitting and held-out scoring took 65.53 seconds after the merge. Complete labels with `seed % 10 == 0` were held out, leaving 33,863 training rows.

| Boss node | Collected roots | Held-out roots | Rows | Draws | Brier | AUC | Same-root ordering | Comparisons |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Act 1 Boss | 2,352 | 227 | 908 | 14,528 | 0.0736 | 0.8201 | 0.6667 | 528 |
| Act 2 Boss | 2,352 | 215 | 860 | 13,760 | 0.1440 | 0.8747 | 0.6351 | 803 |
| Act 3 first Boss | 2,352 | 235 | 940 | 15,040 | 0.1399 | 0.8830 | 0.5879 | 791 |
| Act 3 second Boss | 2,344 | 248 | 989 | 15,824 | 0.1765 | 0.8121 | 0.6015 | 916 |

Brier and AUC use individual rollout outcomes. Ordering compares variants at the same root with unequal observed win rates and gives half credit to a predicted tie.

Fit receipt SHA256 `82bbf0a097284709fa30d7e9ab3041c868c7b8dbf0c0137e470678a362d40c02`. Evaluator weights SHA256 `95a2bd8ac51b1242b098223dbe6dc6eab9c1c2959301ab9f5cf9707c57a25ff9`.

## Matched play

Slurm job 5772 completed with exit 0:0 in 746 seconds on two V100s. Both strategies evaluated seeds 2,000,000,000 through 2,000,001,023 once, with key-last-chance enabled. The paired standard error uses the sample standard deviation of per-seed floor differences divided by the square root of 1,024.

| Outcome | Plain argmax | Deck evaluator |
| --- | ---: | ---: |
| Mean floor | 32.8818 | 26.0205 |
| Act 1 Boss cleared | 851/1024 | 680/1024 |
| Act 2 Boss cleared | 334/1024 | 182/1024 |
| Act 3 first Boss cleared | 95/1024 | 32/1024 |
| Act 3 second Boss cleared | 32/1024 | 5/1024 |
| Shield and Spear cleared | 29/1024 | 2/1024 |
| Heart cleared | 1/1024 | 0/1024 |
| Stalled | 1/1024 | 1/1024 |

Stalled episodes contribute their recorded floor and Boss flags. The audit verified all 16 seed blocks and paired each seed before computing the statistics.

Collection audit SHA256 `5089b9337e2cd3dfe9fab26ed035c9a0a96153b123981bbfbcd595bc5cb6f50e`. Paired audit SHA256 `584d8a32ad7fa0526250e197b05805d332f53d01254d0a076f4ec5741402a73e`.
