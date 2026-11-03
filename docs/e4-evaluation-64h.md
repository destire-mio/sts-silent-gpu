### Large-model 64h fixed-seed evaluation

Update 1333, cumulative training seconds 230452.3569513085: independently recomputed eight × 128 GPU argmax games from fresh starts, fixed seeds 2000000000–2000001023. These fixed-dev results do not establish unseen-seed generalization or a causal effect of a training or key-policy change. No quality gate is added.

| Checkpoint | Mean floor | Act1Boss /1024 | Act2Boss /1024 | First Act3Boss /1024 | Act3 wins /1024 | Heart wins /1024 | Paired teacher floor delta / SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 62h / 1266 | 30.9267578125 | 834 | 244 | 56 | 14 | 0 | 13.45703125 / 0.388527923416344 |
| 64h / 1333 | 30.025390625 | 837 | 204 | 43 | 10 | 0 | 12.5556640625 / 0.37449158466055 |

The same-seed floor change from 62h to 64h is -0.9013671875, paired SE 0.4054879946596411. This checkpoint is lower on this fixed set; a single comparison does not determine a persistent training trend. Act3 wins mean RewardLedger.reach.act3_second_boss; Heart wins mean RewardLedger.reach.heart. The paired teacher is dev-16.pt argmax on the same 1024 seeds.

| Milestone | Games /1024 |
| --- | ---: |
| Ruby key | 11 |
| Emerald key | 127 |
| Sapphire key | 31 |
| All three keys | 1 |
| Entered Act4 | 0 |
| Defeated Shield and Spear | 0 |
| Defeated Heart | 0 |

All 1024 standard episodes and ledger rows agree on keys, reach, floor and outcome. Outcomes are death 1012 / act3_win 10 / stalled 2. The established outcome rule labels Act3 reach without a Heart win as act3_win and omits that row's death field; it does not prove post-Act3 survival or a Heart victory. No Heart-winning game occurred. Key counts reflect acquired terminal bits.

| Student decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.33337030963734016 | 792531 |
| card_reward | 0.15298730263526503 | 26187 |
| map | 0.061223362394630165 | 50446 |
| shop | 0.1455963150147632 | 15653 |
| campfire | 0.05997153092511367 | 24412 |
| other | 0.03535570118843768 | 139347 |

Counts sum to 1048576 and exactly match update 1333. Fresh training completions 1263, mean floor 26.068091844813935; midstart completions 1576, mean floor 36.036802030456855, kept separate from the fresh-start dev examination. Learner sample-passes across two epochs are 2097152. Finite=true, optimizer steps 64, AMP skips 0, episode faults 0, stop=null; four pool sizes 256/256/256/256, value coefficient 181.3911719048255. Preclip norm mean/max 0.489479701500386/0.9111937284469604, clipped fraction 0.390625, logged under the existing 0.5 threshold without adding a stop rule.

The exact [64h per-game Act2 ledger](e4-act2-ledger-64h.json) has 1630122 bytes, SHA-256 eab7fd89e2881bd1cdf2e041748d1a2a1963e27977d200023af77742f3260050. Its 1024 rows match eight standard episode files and eight ledger shards. Deck IDs/upgrades, relics, potion slots, HP/maxHP/gold, death room ID/encounter roster/entry HP/nonempty potions/turn for death-classified rows, explicit skips/net removals/rests/upgrades/elites/stalls remain retained. Encounter rosters do not identify an individual killer, and equal-size transforms do not count as removals.

Independently recomputed average-tie Spearman: Act1Boss marker 962 cases / 0.41655110162072023; Act2-entry marker 837 cases / 0.4545259492550013. These condition on reaching the respective marker. Totals {"card_skips":2849,"card_removals":2489,"rests":3155,"upgrades":2263}; elites by act {"1":1197,"2":749,"3":216,"4":0}; Act2-entry means {"deck_size":20.64516129032258,"hp":57.75149342891278,"max_hp":67.54599761051374,"gold":186.31302270011946}.

Retained dev-64.pt is model-only, strict-loads with configuration 384 × 6 / six heads / FF1536, with 12435074 finite parameters in 49762853 bytes; SHA-256 64ac4d81d9e16b392977661b421fdd3a36ab3b7b68968b3b10a93921dd2acf1d. The snapshot is retained without overwrite; no model binary is published.

Only the new 62→64h LR/KL window was audited: (223210.22540984763, 230452.3569513085], 67 actual PPO updates 1267–1333 from the continuing run. Failed, unsaved attempts with reused IDs remain excluded. LR min/max/current/next are all 0.00006666666666666667, zero at 1e-5; KL min/max/end 0.005148527892436313/0.012601706551188169/0.010765339192255384. Older windows remain closed.

These results use ordinary argmax before expert iteration has started. The authorized same-checkpoint first-256 independent argmax/combat comparison is pending the prepared implementation's next natural startup; it has no result yet. This examination does not measure expert-training effects. The dev-60 dedicated argmax comparison differed from the ordinary first-256 baseline on two seeds for an undetermined reason; those closed results are not rerun or substituted here. Next ordinary evaluation: 66h / 237600 cumulative training seconds, with LR/KL strictly after 230452.3569513085.
