### Large-model 62h fixed-seed evaluation

Update 1266, cumulative training seconds 223210.22540984763: independently recomputed eight × 128 GPU argmax games from fresh starts, fixed seeds 2000000000–2000001023. These are fixed-dev measurements; they do not establish unseen-seed generalization or a causal effect of a training or key-policy change. No quality gate is added.

| Checkpoint | Mean floor | Act1Boss /1024 | Act2Boss /1024 | First Act3Boss /1024 | Act3 wins /1024 | Heart wins /1024 | Paired teacher floor delta / SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 60h / 1199 | 30.6279296875 | 867 | 215 | 40 | 8 | 0 | 13.158203125 / 0.35599975976855525 |
| 62h / 1266 | 30.9267578125 | 834 | 244 | 56 | 14 | 0 | 13.45703125 / 0.388527923416344 |

Act3 wins mean RewardLedger.reach.act3_second_boss; Heart wins mean RewardLedger.reach.heart. The paired teacher is dev-16.pt argmax on the same 1024 seeds.

| Milestone | Games /1024 |
| --- | ---: |
| Ruby key | 23 |
| Emerald key | 84 |
| Sapphire key | 165 |
| All three keys | 4 |
| Entered Act4 | 1 |
| Defeated Shield and Spear | 1 |
| Defeated Heart | 0 |

All 1024 standard episodes and ledger rows agree on keys, reach, floor and outcome. Outcomes are death 1009 / act3_win 14 / stalled 1. Seed 2000000273 has all three keys, Act4 and Shield/Spear reach markers, final floor 56, and no Heart-win marker. No Heart-winning game occurred.

The established outcome rule labels Act3 reach without a Heart win as act3_win and omits that row's death field. All 14 such ledger rows follow that rule; this classification does not mean a Heart victory or prove survival after Act3. Key counts reflect acquired terminal bits.

| Student decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.3157384068798808 | 788240 |
| card_reward | 0.15455480485986592 | 26495 |
| map | 0.06197672238747551 | 51172 |
| shop | 0.13656950730548495 | 15087 |
| campfire | 0.05889688571125687 | 25493 |
| other | 0.032362739335623246 | 142089 |

Counts sum to 1048576 and exactly match update 1266. Fresh training completions 1305, mean floor 27.259003831417626; midstart completions 1529, mean floor 35.487246566383256, kept separate from the fresh-start dev examination. Learner sample-passes across two epochs are 2097150. Finite=true, optimizer steps 64, AMP skips 0, episode faults 0, stop=null; four pool sizes 256/256/256/256, value coefficient 181.3911719048255. Preclip norm mean/max 1.5806638030335307/2.9175407886505127, clipped fraction 1.0, logged under the existing 0.5 threshold without adding a stop rule.

The exact [62h per-game Act2 ledger](e4-act2-ledger-62h.json) has 1636825 bytes, SHA-256 7c3154612f3dbf3cb8de123a696e19f5262fdc917319080886ca31362cce556e. Its 1024 rows match eight standard episode files and eight ledger shards. Deck IDs/upgrades, relics, potion slots, HP/maxHP/gold, death room ID/encounter roster/entry HP/nonempty potions/turn for death-classified rows, explicit skips/net removals/rests/upgrades/elites/stalls remain retained. Encounter rosters do not identify an individual killer, and equal-size transforms do not count as removals.

Independently recomputed average-tie Spearman: Act1Boss marker 965 cases / 0.4901319394303247; Act2-entry marker 834 cases / 0.5193641635819277. These condition on reaching the respective marker. Totals {"card_skips":3136,"card_removals":2460,"rests":3209,"upgrades":2489}; elites by act {"1":1362,"2":868,"3":256,"4":1}; Act2-entry means {"deck_size":20.844124700239806,"hp":57.88369304556355,"max_hp":67.7062350119904,"gold":191.11390887290167}.

Retained dev-62.pt is model-only, strict-loads with configuration 384 × 6 / six heads / FF1536, with 12435074 finite parameters in 49762853 bytes; SHA-256 dd379738c91dab937bf028b18f39e1a7e15bb087713f1b26425339cdc8097421. The snapshot is retained without overwrite; no model binary is published.

Only the new 60→62h LR/KL window was audited: (216055.2297527986, 223210.22540984763], 67 actual PPO updates 1200–1266. The retained lineage uses 28 updates 1200–1227 before a natural handoff and 39 updates 1228–1266 from the actual continuing run. Failed, unsaved attempts with reused update IDs are excluded. LR min/max/current/next are all 0.00006666666666666667, zero at 1e-5; KL min/max/end 0.0056639843344703095/0.013375422016017807/0.0062227584667569015. Older windows remain closed.

These results use ordinary argmax without expert iteration. The dev-60 lookahead comparison is already published separately; the authorized expert-training implementation awaits its natural resume. RewardLedger, PPO learner and model settings for this examination are unchanged. Next natural evaluation: 64h / 230400 cumulative training seconds, with LR/KL strictly after 223210.22540984763. At the authorized eight-hour boundary, a separate same-checkpoint first-256 argmax/combat comparison is also scheduled by the prepared implementation.
