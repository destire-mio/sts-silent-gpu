### Large-model 68h fixed-seed evaluation (observation v13)

Update 1443, cumulative training seconds 244840.0398180486: eight × 128 fresh-start GPU argmax games on fixed seeds 2000000000–2000001023. The actual worker uses observation v13 and the encoder activation-memory repair. The prepared observation-v14 and throughput-controller changes have not entered this worker. These fixed-seed results do not isolate an expert-training effect or establish unseen-seed generalization.

| Checkpoint | Mean floor | Act1Boss /1024 | Act2Boss /1024 | First Act3Boss /1024 | Act3 wins /1024 | Heart wins /1024 | Paired teacher floor delta / SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 66h /1394 |30.1650390625|830|225|51|10|0|12.6953125 /0.39172704696995875|
| 68h /1443 |29.7373046875|830|211|42|9|0|12.267578125 /0.37976652043206327|

The same-seed floor change from66h is -0.427734375, paired SE 0.3903131726339968. This alone does not establish sustained degradation or its cause. Act3 wins mean RewardLedger.reach.act3_second_boss; Heart wins mean RewardLedger.reach.heart. The paired teacher is dev-16.pt argmax on the same1024 seeds.

| Milestone | Games /1024 |
| --- | ---: |
| Ruby key |15|
| Emerald key |185|
| Sapphire key |38|
| All three keys |5|
| Entered Act4 |0|
| Defeated Shield and Spear |0|
| Defeated Heart |0|

All standard episodes and ledger rows agree on floor, keys, reach and outcome. Outcomes: death1014 /act3_win9 /stalled1. The established classification omits the death field from act3_win rows; its absence does not establish post-Act3 survival or a Heart victory. Key counts reflect acquired terminal bits.

| Student decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.11984204652117032 | 790094 |
| card_reward | 0.06637699935685816 | 26458 |
| map | 0.03636250420053282 | 50660 |
| shop | 0.06823128751276422 | 15376 |
| campfire | 0.026124233474892664 | 24661 |
| other | 0.01901566732764117 | 141327 |

The six student counts sum to1048576 and match update1443; valid mainline decisions1048576, learner sample-passes across two epochs2097152. Fresh training completions1144, mean floor29.232517482517483; midstart completions1446, mean floor37.03042876901798, separate from the fresh-start examination. Finite=true, optimizer steps64, AMP skips0, faults0/18937, stop=null; four pools256/256/256/256, value coefficient181.3911719048255. Preclip norm mean/max33.409810999408364/97.0220947265625, clipped fraction1, logged under the existing0.5 clip without a new stop rule.

The exact [68h per-game Act2 ledger](e4-act2-ledger-68h.json) has 1645246 bytes, SHA-256 62ccdb10bc8178451126f35f75ca367a4d06b3b65bf536da47835863ed31b689. Its1024 rows match eight standard episode files and eight ledger shards. Deck IDs/upgrades, relics, potion slots, HP/maxHP/gold, death room ID/encounter roster/entry HP/nonempty potions/turn for death-classified rows, explicit skips/net removals/rests/upgrades/elites/stalls are retained. Encounter rosters do not identify an individual killer; equal-size transforms do not count as removals.

Independently recomputed average-tie Spearman: Act1Boss marker940 cases /0.5049500196723906; Act2-entry marker830 cases /0.5036696192361897. These condition on reaching the respective marker. Totals {"card_skips":2712,"card_removals":2371,"rests":3405,"upgrades":2016}; elites by act {"1":1334,"2":783,"3":232,"4":0}; Act2-entry means {"deck_size":21.11686746987952,"hp":58.02168674698795,"max_hp":67.62289156626505,"gold":189.74096385542168}.

Retained dev-68.pt is model-only and strict-loads with configuration384 ×6 /six heads /FF1536,12435074 finite parameters,49762853 bytes; SHA-256 8dcc27c4c5f792bf195d4db061286f978989ac66fcaa6c36e08c6c36f4a2c53b. The snapshot is retained without overwrite; no model binary is published. It is distinct from a shared model-plus-Adam recovery checkpoint.

Only the new66→68h LR/KL window was audited: (237713.67285748757,244840.0398180486],49 actual PPO updates1395–1443. Lineage:4534 updates1395–1414 (20),4548 updates1415–1443 (29);4535 has no PPO updates. The saved partial54 was already applied to Adam before4548 resumed and is carried once, not reapplied or charged twice. Earlier failed jobs' orphan attempts remain excluded and their allocation charges remain counted once. LR min/max/end/next 0.000013168724279835392/0.000019753086419753087/0.000013168724279835392/0.000013168724279835392, zero at1e-5; KL min/max/end 0.0049665817519846675/0.02650012415652738/0.014606973439244018. Older windows remain closed.

At1443: expert samples181, beta0.5, CE0.7843184245773265, argmax agreement0.8011049723756906, L2/storage3, extra/base18.66312593605835/127.57061393593904s, overhead0.14629643426683075. The fixed pre-expert anchor remains16800940032 bytes; logged growth limit16861233152, measured expert peak16870146048, which exceeds the logged limit by8912896 bytes. This is a recorded limitation, not a complete physical-memory-cap or512-lane certification. Expert labels remain separate from PPO/value rows.

Observation v14 and the throughput controller are prepared and validated for a later natural resume. This68h examination is still v13. The closed dev64 lookahead window is not rerun. Next ordinary evaluation:70h /252000 cumulative training seconds, LR/KL strictly after244840.0398180486. Next periodic combat comparison:72h, first128 seeds with paired completed intersections.
