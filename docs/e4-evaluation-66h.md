### Large-model 66h fixed-seed evaluation

Update 1394, cumulative training seconds 237713.67285748757: independently recomputed eight × 128 fresh-start GPU argmax games on fixed seeds 2000000000–2000001023. This is the first ordinary examination after actual combat-expert training. It does not isolate an expert-training effect or establish unseen-seed generalization. No quality gate is added.

| Checkpoint | Mean floor | Act1Boss /1024 | Act2Boss /1024 | First Act3Boss /1024 | Act3 wins /1024 | Heart wins /1024 | Paired teacher floor delta / SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 64h /1333 | 30.025390625 |837|204|43|10|0|12.5556640625 /0.37449158466055 |
| 66h /1394 | 30.1650390625 |830|225|51|10|0|12.6953125 /0.39172704696995875 |

The same-seed floor change from 64h is 0.1396484375, paired SE 0.4009828847075043. These results alone do not demonstrate an improvement or a causal effect. Act3 wins mean RewardLedger.reach.act3_second_boss; Heart wins mean RewardLedger.reach.heart. The paired teacher is dev-16.pt argmax on the same 1024 seeds.

| Milestone | Games /1024 |
| --- | ---: |
| Ruby key |16|
| Emerald key |182|
| Sapphire key |32|
| All three keys |2|
| Entered Act4 |1|
| Defeated Shield and Spear |1|
| Defeated Heart |0|

All standard episodes and ledger rows agree on floor, keys, reach and outcome. Outcomes: death1012 /act3_win10 /stalled2. Seed2000000120 reaches floor56 with all three keys, Act4 and Shield/Spear reach bits, Heart=false; outcome=act3_win. The established outcome rule classifies Act3 reach without a Heart win as act3_win and omits its death field. This does not establish post-Act3 survival or a Heart victory. Key counts reflect acquired terminal bits.

| Student decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.13970890154299673 | 793804 |
| card_reward | 0.08281816281704288 | 26206 |
| map | 0.04049298072723297 | 50031 |
| shop | 0.08074099250377179 | 15018 |
| campfire | 0.028579497261347633 | 24155 |
| other | 0.023487308185467154 | 139362 |

The six counts sum to1048576 and match update1394. The valid mainline-decision total is1048575 and learner sample-passes across two epochs are2097150, with one invalid episode row excluded from learning. Fresh training completions 1144, mean floor 29.255244755244757; midstart completions 1464, mean floor 37.17213114754098, kept separate from the fresh-start dev examination. Finite=true, optimizer steps64, AMP skips0, one episode fault in18939 cases (0.000052801098262843864), stop=null; four pools256/256/256/256, value coefficient181.3911719048255. Preclip norm mean/max 30.131193947978318/138.93240356445312, clipped fraction1, logged under the existing0.5 threshold without a new stop rule.

The exact [66h per-game Act2 ledger](e4-act2-ledger-66h.json) has 1633815 bytes, SHA-256 388c1e5806d275f5f94d3bc9c99773fd643fe0aeebf35e57509182f7d2d27e72. Its1024 rows match eight standard episode files and eight ledger shards. Deck IDs/upgrades, relics, potion slots, HP/maxHP/gold, death room ID/encounter roster/entry HP/nonempty potions/turn for death-classified rows, explicit skips/net removals/rests/upgrades/elites/stalls are retained. Encounter rosters do not identify an individual killer; equal-size transforms do not count as removals.

Independently recomputed average-tie Spearman: Act1Boss marker947 cases /0.4888175914642239; Act2-entry marker830 cases /0.4973766383784493. These condition on reaching the respective marker. Totals {"card_skips":2708,"card_removals":2511,"rests":3328,"upgrades":2164}; elites by act {"1":1220,"2":750,"3":258,"4":1}; Act2-entry means {"deck_size":20.880722891566265,"hp":57.7289156626506,"max_hp":67.53373493975904,"gold":191.9325301204819}.

Retained dev-66.pt is model-only, strict-loads with configuration384 ×6 /six heads /FF1536,12435074 finite parameters,49762853 bytes; SHA-256 ca024aa2a01cd35b989a6390dfcaef894a6226af0b69f3670b9d160989137c55. The snapshot is retained without overwrite; no model binary is published. This model snapshot does not imply that the shared model-plus-Adam recovery checkpoint has already advanced.

Only the new64→66h LR/KL window was audited: (230452.3569513085,237713.67285748757],61 actual PPO updates1334–1394. Lineage:4488 updates1334–1355 (22),4494 updates1356–1375 (20),4515 updates1376–1391 (16),4534 updates1392–1394 (3). Jobs4489/4508 add no PPO rows. Each of4515,4517,4525,4526,4527,4528,4529,4530,4531,4532,4533 has a separate failed1392–1393/unfinished1394 attempt, excluded even though IDs match4534. Historical allocation charges remain counted once. LR min/max/end/next 0.000013168724279835392/0.00006666666666666667/0.000013168724279835392/0.000013168724279835392, zero at1e-5; KL min/max/end 0.005107852350446737/0.054892500177155634/0.0073232231408402425. Older windows remain closed.

Actual expert updates begin at1357. At1394: samples107, beta0.5, CE0.8348472427136644, argmax agreement0.8317757009345794, L1/storage2, extra/base27.024419638881227/131.05345651411335s, overhead0.20620913295767154. The fixed pre-expert anchor remains16800940032 bytes, logged growth limit16861233152; this row's measured expert peak16498950144 is a logged observation, not a complete physical-memory-cap or512-lane certification. Expert labels remain separate from PPO/value rows.

The final three rounds use encoder activation recomputation to lower retained learner activations while preserving model size, microbatch512 and losses. Update1394 completes with64 Adam steps/zero AMP skips. Rank0 PyTorch peak allocation is5149400064 bytes; first resumed1392 has5053864448 versus8761127424 for the corresponding earlier attempt, and146.698s versus127.834s elapsed. These observations do not guarantee every future round's peak or attribute every earlier OOM to one cause.

Observation v14 is not yet active. This completed ordinary examination satisfies its waiting gate; server validation and a later natural resume remain separate. The closed dev64 lookahead window is not rerun. Next ordinary evaluation:68h /244800 cumulative training seconds, with LR/KL strictly after237713.67285748757. Next periodic combat comparison:72h, first128 seeds with paired completed intersections.
