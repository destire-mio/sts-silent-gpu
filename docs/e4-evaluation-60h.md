### Large-model 60h fixed-seed evaluation

Update 1199, cumulative training seconds 216055.2297527986: independently recomputed eight × 128 GPU argmax games from fresh starts, fixed seeds 2000000000–2000001023. These are fixed-dev measurements; they do not establish unseen-seed generalization or a causal effect of the key-rule change. No quality gate is added.

| Checkpoint | Mean floor | Act1Boss /1024 | Act2Boss /1024 | First Act3Boss /1024 | Act3 wins /1024 | Heart wins /1024 | Paired teacher floor delta / SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 58h / 1132 | 30.294921875 | 832 | 232 | 41 | 7 | 0 | 12.8251953125 / 0.38173593241980563 |
| 60h / 1199 | 30.6279296875 | 867 | 215 | 40 | 8 | 0 | 13.158203125 / 0.35599975976855525 |

Act3 wins mean RewardLedger.reach.act3_second_boss; Heart wins mean RewardLedger.reach.heart. The paired teacher is dev-16.pt argmax on the same 1024 seeds.

| Milestone | Games /1024 |
| --- | ---: |
| Ruby key | 16 |
| Emerald key | 86 |
| Sapphire key | 17 |
| All three keys | 0 |
| Entered Act4 | 0 |
| Defeated Shield and Spear | 0 |
| Defeated Heart | 0 |

All 1024 standard episodes and ledger rows agree on keys, reach, floor and outcome. Outcomes are death 1013 / act3_win 8 / stalled 3; all eight Act3-win ledger rows have no death record. No Heart-winning game occurred. Positive Heart-victory outcome behavior is covered by the CPU regression rather than a positive example in this examination. Key counts reflect acquired terminal bits.

| Student decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.2855734402867482 | 784610 |
| card_reward | 0.14515922137784984 | 26900 |
| map | 0.05573883149505808 | 51799 |
| shop | 0.11528706825101642 | 16130 |
| campfire | 0.04668088145571388 | 25217 |
| other | 0.027800442408365166 | 143920 |

Counts sum to 1048576 and exactly match update 1199. Fresh training completions 1173, mean floor 28.48678601875533; midstart completions 1578, mean floor 36.2807351077313, kept separate. Finite=true, optimizer steps 64, AMP skips 0, episode faults 0, stop=null; four pool sizes 256/256/256/256, value coefficient 181.3911719048255. Preclip norm mean/max 0.4312070100568235/0.706298291683197, clipped fraction 0.265625, logged under the existing 0.5 threshold.

The exact [60h per-game Act2 ledger](e4-act2-ledger-60h.json) has 1662358 bytes, SHA-256 d6605797534ceddd5143dcc5766ec4a7368eb24fa189d220cad261928ae0a744. Its 1024 rows match eight standard episode files and eight ledger shards. Deck IDs/upgrades, relics, potion slots, HP/maxHP/gold, death room ID/encounter roster/entry HP/nonempty potions/turn, explicit skips/net removals/rests/upgrades/elites/stalls remain retained. Encounter rosters do not identify an individual killer, and equal-size transforms do not count as removals.

Independently recomputed average-tie Spearman: Act1Boss marker 966 cases / 0.41235209048624133; Act2-entry marker 867 cases / 0.47937882691399. These condition on reaching the respective marker. Totals {"card_skips":3520,"card_removals":2499,"rests":3224,"upgrades":2296}; elites by act {"1":1140,"2":748,"3":202,"4":0}; Act2-entry means {"deck_size":20.562860438292965,"hp":57.63667820069204,"max_hp":67.46712802768167,"gold":187.09457900807382}.

Retained dev-60.pt is model-only, strict-loads with configuration 384 × 6 / six heads / FF1536, with 12435074 finite parameters in 49762853 bytes; SHA-256 2bdf1258e7900458d71d4902f599c7b6483309cc4570c15e8cf464267b87b40e. The snapshot is retained without overwrite; no model binary is published.

Only the new 58→60h LR/KL window was audited: (208889.59595534558, 216055.2297527986], 67 committed PPO updates 1133–1199 in one continuous segment. Unsaved orphan updates with reused IDs are excluded. LR min/max/current/next 0.00006666666666666667/0.0001/0.00006666666666666667/0.00006666666666666667, zero at 1e-5; KL min/max/end 0.006088144088709534/0.021380732464887746/0.012327979213227744. Older windows remain closed.

RewardLedger, learner and model settings remain unchanged. Next natural evaluation: 62h / 223200 cumulative training seconds, with LR/KL strictly after 216055.2297527986. The separately authorized dev-60 first-256 argmax/turn/combat comparison is awaiting its natural continuation window; no lookahead timing or result is claimed here. Training search and 512-model training remain unstarted.
