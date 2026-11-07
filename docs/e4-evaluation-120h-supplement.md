# E4 120-hour supplement

The supplement used the same 384×6 dev-120 checkpoint as the [ordinary 120-hour evaluation](e4-evaluation-120h.md), at PPO update 2899 and training clock 432097.7652507751 s. Four V100s handled eight logical shards per natural window. The comparison used seeds 2000000000–2000000127, with argmax and combat lookahead as the two evaluation modes.

| Mode | Completed | Mean floor among completed | Heart wins | Recorded mode seconds |
| --- | ---: | ---: | ---: | ---: |
| Argmax | 128 / 128 | 30.8828125 | 0 | 46.02211871184409 |
| Combat | 124 / 128 | 35.435483870967744 | 0 | 3600.002231305116, lower bound |

Combat reached the one-hour cap in the retained timer and finalized with 124 results. Logical shard 6 had four time-censored seeds, 2000000006, 2000000022, 2000000030 and 2000000094. The comparison below is conditional on the 124 seeds completed by both modes.

| Shared 124 seeds | Mean floor | Act 1 boss | Act 2 boss | Act 3 first boss | Act 3 second-boss flag | Heart |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Argmax | 30.975806451612904 | 105 | 29 | 6 | 2 | 0 |
| Combat | 35.435483870967744 | 108 | 49 | 16 | 3 | 0 |

The paired combat gain was **4.459677419354839 floors**, with SE **1.0259803186003662**. Combat reached a higher floor on 57 seeds, tied on 40 and reached a lower floor on 27.

| Combat window | Job | Runtime | Retained parallel seconds | Window outcome |
| --- | ---: | --- | ---: | --- |
| 1794086092471370299 | 5388 | `30eb3357` | 145.22931608604267 | Saved progress at natural allocation end |
| 1794086463140964488 | 5389 | `364e8e4b` | 2504.620977214072 | Preview forward index error on physical rank 2, logical shard 6 |
| 1794089162843786826 | 5390 | `364e8e4b` | 950.1519380050013 | Reached the recorded cap |

For each window, the timer sums logical shards serially on each physical worker, then takes the maximum across workers. The crash leaves the combat timer incomplete, so the recorded 3600.002231305116 s is a lower bound on aggregate mode wall time.

Combat counters recorded 115,036 mainline decisions, 45,648 preview decisions, 7,895 overrides and 34,362,085 preview steps across the retained work, including incomplete prefixes. The preview decision share was 39.68%, and 17.30% of preview decisions produced overrides. Per-preview SE-zero frequency is N/A because the retained records lack per-preview SE values.

One independent CPU audit reconciled 55 JSON files, the dev-120 digest, the mode counters, the timing receipts and all paired results. It took 0.24646329879760742 s and exited with code 0 and empty stderr. It used the original evaluation outputs and performed zero parameter updates.

| Artifact | SHA-256 |
| --- | --- |
| dev-120.pt | `eb7eba86962f495b3c9e8c09775cf1e262163c5d1e4d44f88ef20cdcdec9ee80` |
| Original supplement report | `40e5461c9c8a3452528267005fe4f77d79a1da82a22d7bc5b563123cbec3cbd4` |
| Independent audit | `fe8c8a936ad39b3b2f65251c06412dd43994cd711ba2e3bb3bc39433d303243f` |
