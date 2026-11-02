### Large-model 56h fixed-seed evaluation

Update 1064, cumulative training seconds 201617.64553146454: independently recomputed 8 × 128 GPU argmax games, fresh starts on fixed seeds 2000000000–2000001023, without saved or mid-game evaluation starts. This is fixed-dev measurement; it does not establish unseen-seed generalization or causality and adds no quality gate.

| Evaluation | Mean floor | Act1Boss /1024 | Act2Boss /1024 | First Act3Boss /1024 | Victories | Paired teacher delta / SE |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 54h / 996 | 30.09375 | 855 | 205 | 23 | 0 | 12.6240234375 / 0.35814901892582024 |
| 56h / 1064 | 30.208984375 | 860 | 212 | 27 | 0 | 12.7392578125 / 0.3725243380020542 |

The paired baseline is dev-16.pt argmax on the same 1024 seeds. The 54h row is the previously audited result.

| Student decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.29430445612065037 | 785125 |
| card_reward | 0.1462167235979104 | 27389 |
| map | 0.0523393093675243 | 52251 |
| shop | 0.12238703104035079 | 14566 |
| campfire | 0.0501449048705309 | 23453 |
| other | 0.031149391125550412 | 145792 |

Counts sum to 1048576 and exactly match update 1064. Fresh training completions 1235, mean floor 28.211336032388665; midstart completions 1538, mean floor 36.213914174252274, reported separately. Finite=true; optimizer steps 64, AMP skips 0, episode faults 0, stop=null. Four pools have sizes 256/256/256/256; value coefficient 181.3911719048255. Preclip norm mean/max 0.5316479930188507/1.026037335395813, clipped fraction 0.5, logged under the existing 0.5 threshold.

The [56h per-game Act2 ledger](e4-act2-ledger-56h.json) is the exact 1409368-byte worker JSON, SHA-256 872d3dc76faf5b86d214349ad83e6e7996011eb63286fa0737a76592c73163e7. All 1024 seed/floor/outcome records match eight standard episode files and eight ledger shards. Its 860 Act2-entry snapshots retain deck IDs/upgrades, relics, potion slots, HP/maxHP/gold; death records retain room ID, encounter roster, entry HP, nonempty potions and turn. Explicit skips, net removals, rests, upgrades, elites by act and stalled games are retained. Encounter rosters do not identify an individual killer; equal-size transforms do not count as removals.

Independently recomputed average-tie Spearman: Act1Boss marker 957 cases / 0.46737685391769834; Act2-entry marker 860 cases / 0.4679748875584168. These correlations condition on reaching each marker. Outcomes {"death":1023,"stalled":1}; counts {"card_skips":3119,"card_removals":2385,"rests":3761,"upgrades":1641}; elites by act {"1":1215,"2":789,"3":210,"4":0}; Act2-entry means {"deck_size":20.56046511627907,"hp":58.217441860465115,"max_hp":67.54186046511627,"gold":188.78255813953487}.

Retained dev-56.pt is model-only, strict-loads with configuration 384 × 6 / 6 heads / FF1536, and contains 12435074 finite parameters in 49762853 bytes. SHA-256 6e3520ec65b4a9b70faecb70aa59af404d5fe6a93105171942a83ab826676221. The model binary remains private and the snapshot is retained without overwrite.

Only the new 54→56h LR/KL window was audited: (194421.85996289173, 201617.64553146454], 68 committed PPO updates 997–1064. LR min/max/current/next 0.0001/0.0001/0.0001/0.0001; zero updates at 1e-5. KL min/max/end 0.006383698222353977/0.017658558826562573/0.010207277219050934. Source segments are 997–1000, 1001–1039, 1040–1043, 1044–1047 and 1048–1064, retained separately by actual job and phase in the audit. Resumed partial optimizer steps are applied once; they do not create duplicate LR rows. Earlier windows remain closed.

The existing four-pool source and completed value calibration continue. No GPU lookahead, 512-model training or new distillation was launched. The next natural evaluation is 58h / 208800 cumulative training seconds, with its new LR/KL window strictly after 201617.64553146454.
