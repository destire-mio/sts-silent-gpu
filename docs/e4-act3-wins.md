### Act 3 wins: corrected evaluation definition

For the keyless evaluation route, a win means defeating the second Act 3 boss: `RewardLedger.reach["act3_second_boss"]`. The engine then enters HeartEvent (room 11) and a Death screen rather than Screen::Victory. Counting only `outcome == "victory"` therefore reported zero. Future evaluation episode and ledger outcomes use `act3_win` for this marker; the training reward calculation is unchanged.

The existing eight episode shards per checkpoint were counted again without replaying games or running a new evaluation. In all five checkpoints, the marker seeds exactly match the retained floor-52 / room-11 / empty-encounter / turn-1 records. The reach marker is the counting rule; that terminal signature is a cross-check.

| Fixed-dev checkpoint | Update | Episodes | Act 3 wins | Win rate | Old Victory-screen count |
| --- | ---: | ---: | ---: | ---: | ---: |
| 48h | 788 | 1024 | 0 | 0% | 0 |
| 50h | 858 | 1024 | 1 | 0.09765625% | 0 |
| 52h | 927 | 1024 | 0 | 0% | 0 |
| 54h | 996 | 1024 | 5 | 0.48828125% | 0 |
| 56h | 1064 | 1024 | 7 | 0.68359375% | 0 |

These counts supersede the victories columns in the [48–54h report](e4-evaluation-48h-54h.md) and [56h report](e4-evaluation-56h.md). Their other measurements and original per-game JSON remain unchanged; the old death outcome in those raw records is the engine-screen classification. Under the corrected definition, the 50h outcomes are death 1022 / stalled 1 / act3_win 1; 54h death 1017 / stalled 2 / act3_win 5; 56h death 1016 / stalled 1 / act3_win 7. The 48h and 52h outcomes do not change.

All rows use the same 1024 fixed seeds 2000000000–2000001023, 8 × 128 fresh GPU argmax games. Recounting does not establish unseen-seed generalization or causality. No model, training reward, learner, collection policy or checkpoint transaction is changed by this reporting correction. The new source takes effect at the next natural job handoff; an already running worker keeps its imported source until then.
