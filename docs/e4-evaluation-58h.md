### Large-model 58h fixed-seed evaluation

Update 1132, cumulative training seconds 208889.59595534558: independently recomputed eight × 128 GPU argmax games from fresh starts, fixed seeds 2000000000–2000001023. This is fixed-dev measurement, not unseen-seed generalization or a causal comparison of the key-rule change; no quality gate is added.

| Checkpoint | Mean floor | Act1Boss /1024 | Act2Boss /1024 | First Act3Boss /1024 | Act3 wins /1024 | Heart wins /1024 | Paired teacher floor delta / SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 56h / 1064 | 30.208984375 | 860 | 212 | 27 | 7 | unavailable in old summary | 12.7392578125 / 0.3725243380020542 |
| 58h / 1132 | 30.294921875 | 832 | 232 | 41 | 7 | 0 | 12.8251953125 / 0.38173593241980563 |

The 56h Act3 win count uses the previously published [definition correction](e4-act3-wins.md). Act3 wins mean RewardLedger.reach.act3_second_boss; heart_wins means RewardLedger.reach.heart. The paired teacher is dev-16.pt argmax on the same 1024 seeds.

| New 58h milestone | Games /1024 |
| --- | ---: |
| Ruby key | 20 |
| Emerald key | 126 |
| Sapphire key | 82 |
| All three keys | 2 |
| Entered Act4 | 0 |
| Defeated Shield and Spear | 0 |
| Defeated Heart | 0 |

All 1024 standard episodes and ledger rows agree on keys, reach, floor and outcome. Outcomes are death 1015 / act3_win 7 / stalled 2; all seven Act3-win ledger rows have no death record. There is no Heart-winning game here, so positive Heart-victory outcome behavior remains covered by the CPU regression rather than demonstrated by this examination. Key counts reflect acquired terminal bits; collecting all keys alone is not a claim that a game reached Act4.

| Student decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.2824520365897696 | 794262 |
| card_reward | 0.14455023537756787 | 26656 |
| map | 0.05847224806400047 | 49663 |
| shop | 0.1258085676140849 | 15086 |
| campfire | 0.048256492579030155 | 23517 |
| other | 0.028149988118753407 | 139392 |

Counts sum to 1048576 and exactly match update 1132. Fresh training completions 1194, mean floor 29.401172529313232; midstart completions 1414, mean floor 37.05940594059406, kept separate. Finite=true, optimizer steps 64, AMP skips 0, episode faults 0, stop=null; four pool sizes 256/256/256/256, value coefficient 181.3911719048255. Preclip norm mean/max 1.5480549251660705/2.807565212249756, clipped fraction 0.921875, logged under the existing 0.5 threshold.

The exact [58h per-game Act2 ledger](e4-act2-ledger-58h.json) has 1657697 bytes, SHA-256 de2916fda611e2beb4bc4f19e49f2502be44ee74b5f17c1343a15b175400bb7c. Its 1024 rows match eight standard episode files and eight ledger shards. Requested deck IDs/upgrades, relics, potion slots, HP/maxHP/gold, death room ID/encounter roster/entry HP/nonempty potions/turn, explicit skips/net removals/rests/upgrades/elites/stalls remain retained. Encounter rosters do not identify an individual killer, and equal-size transforms do not count as removals.

Independently recomputed average-tie Spearman: Act1Boss marker 945 cases / 0.46620879417769556; Act2-entry marker 832 cases / 0.45692301624673326. These condition on reaching the respective marker. Totals {"card_skips":2922,"card_removals":2300,"rests":3177,"upgrades":2248}; elites by act {"1":1280,"2":774,"3":232,"4":0}; Act2-entry means {"deck_size":21.388221153846153,"hp":57.6875,"max_hp":67.6141826923077,"gold":187.53966346153845}.

Retained dev-58.pt is model-only, strict-loads with configuration 384 × 6 / six heads / FF1536, with 12435074 finite parameters in 49762853 bytes; SHA-256 5b2568021c6b31f5199b9a393214a98c76430ba3a394da4dec47baab5fc2398e. The snapshot is retained without overwrite; no model binary is published.

Only the new 56→58h LR/KL window was audited: (201617.64553146454, 208889.59595534558], 68 committed PPO updates 1065–1132. The earlier job contributes committed 1065–1099 (35), and the resumed job contributes its actual 1100–1132 (33). Earlier unsaved orphan updates with reused IDs are excluded. LR min/max/current/next 0.0001/0.0001/0.0001/0.0001, zero at 1e-5; KL min/max/end 0.007028307272607013/0.019698726543258906/0.00981822633049737. Older windows remain closed.

The key choices and evaluation columns are now exercised under the existing natural continuation. RewardLedger, learner and model settings remain unchanged. Next natural evaluation: 60h / 216000 cumulative training seconds, with LR/KL strictly after 208889.59595534558. No GPU lookahead, 512-model training or new distillation was launched.
