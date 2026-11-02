### Large-model48–54h: audited fixed-seed curve

Each row is an independently recomputed8×128 fresh GPU argmax examination on fixed seeds2000000000–2000001023, with no saved or mid-game evaluation starts. All four retained model-only snapshots strict-load with12435074 finite parameters and49762853 bytes. These are fixed-dev results, without a new quality gate or an unseen-seed generalization claim.

| Hours / update | Training seconds | Mean floor | Act1Boss /1024 | Act2Boss /1024 | First Act3Boss /1024 | Victories | Paired teacher floor delta / SE |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 48 / 788 | 172885.2867859129 | 26.888671875 | 834 | 80 | 2 | 0 | 9.4189453125 / 0.30370579301727596 |
| 50 / 858 | 180031.14018304192 | 27.7392578125 | 846 | 109 | 6 | 0 | 10.26953125 / 0.32239290966402445 |
| 52 / 927 | 187208.44305409092 | 28.431640625 | 841 | 148 | 10 | 0 | 10.9619140625 / 0.3376312076499215 |
| 54 / 996 | 194421.85996289173 | 30.09375 | 855 | 205 | 23 | 0 | 12.6240234375 / 0.35814901892582024 |

#### 48h student telemetry and actual ledger

| Decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.2848620105026163 | 759862 |
| card_reward | 0.11713023651181913 | 28769 |
| map | 0.05198389267856149 | 57101 |
| shop | 0.10791075943487209 | 16727 |
| campfire | 0.0485722674749315 | 25577 |
| other | 0.027178330748013884 | 160540 |

Counts sum1048576 and exactly match update788. Fresh training completions1739/mean floor25.618171362852213; midstart1680/mean30.304761904761904, reported separately. Finitetrue, round optimizer steps64, AMP skips0, episode faults0, stopnull; rank0 pools256/256/256, value coefficient181.3911719048255. Preclip norm mean/max0.7575237574055791/1.4495083093643188, clipped rank-attempt fraction0.71875, logged under the existing threshold.

The [48h per-game Act2 ledger](e4-act2-ledger-48h.json) is the exact1394295-byte worker JSON, SHA-2560d8ab404972e11779dc7e39fbbcaccb7bd800169330b6bb335bdb7f5d83a3a6f. All1024 seed/floor/outcome records match the8 standard episode files and8 ledger shards. There are834 Act2-entry snapshots; independently recomputed tied Spearman: Act1Boss marker941 cases/0.44128686570068343, Act2-entry marker834 cases/0.45667968357049493. These condition on reaching the marker. Requested deck IDs/upgrades, relics/potion slots/HP/maxHP/gold, death room ID/encounter roster/entryHP/nonempty potions/turn, explicit skips/net removals/rests/upgrades/elites/stalls are retained; encounter rosters do not identify an individual killer and equal-size transforms do not count as removals.

Ledger outcomes{"death":1017,"stalled":7}; counts{"card_skips":2041,"card_removals":1281,"rests":3131,"upgrades":1532}; elites by act{"1":1154,"2":712,"3":49,"4":0}; Act2-entry means{"deck_size":21.10191846522782,"hp":59.01918465227818,"max_hp":68.72541966426859,"gold":187.64148681055156}. Retained private dev-48.pt SHA-25635de6a4926a5a8fef39fa3d6636b5a7647719b47fee4a762c256b15e42800851; no model binary is published.

#### 50h student telemetry and actual ledger

| Decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.30202718796812283 | 768679 |
| card_reward | 0.13320527070888796 | 28538 |
| map | 0.05031771223589594 | 56225 |
| shop | 0.11824420174319114 | 13906 |
| campfire | 0.04603307951176738 | 25229 |
| other | 0.026659529920433737 | 155999 |

Counts sum1048576 and exactly match update858. Fresh training completions1654/mean floor26.171704957678354; midstart1616/mean30.90408415841584, reported separately. Finitetrue, round optimizer steps64, AMP skips0, episode faults0, stopnull; rank0 pools256/256/256, value coefficient181.3911719048255. Preclip norm mean/max0.5607635495252907/1.2203634977340698, clipped rank-attempt fraction0.515625, logged under the existing threshold.

The [50h per-game Act2 ledger](e4-act2-ledger-50h.json) is the exact1435934-byte worker JSON, SHA-25618ec0f593dd0b76ba39e75dd3e5409de143e599da3d078372ead6cbf63046edd. All1024 seed/floor/outcome records match the8 standard episode files and8 ledger shards. There are846 Act2-entry snapshots; independently recomputed tied Spearman: Act1Boss marker947 cases/0.46369900940595615, Act2-entry marker846 cases/0.4890813505764671. These condition on reaching the marker. Requested deck IDs/upgrades, relics/potion slots/HP/maxHP/gold, death room ID/encounter roster/entryHP/nonempty potions/turn, explicit skips/net removals/rests/upgrades/elites/stalls are retained; encounter rosters do not identify an individual killer and equal-size transforms do not count as removals.

Ledger outcomes{"death":1023,"stalled":1}; counts{"card_skips":1898,"card_removals":1309,"rests":3288,"upgrades":1606}; elites by act{"1":1214,"2":746,"3":67,"4":0}; Act2-entry means{"deck_size":21.706855791962173,"hp":58.88770685579196,"max_hp":68.77068557919621,"gold":189.2434988179669}. Retained private dev-50.pt SHA-256c3b003f03f22e39021eb5278d0c075070dcd5919f2a5ab69a6012170fec8c142; no model binary is published.

#### 52h student telemetry and actual ledger

| Decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.29759642030610634 | 769014 |
| card_reward | 0.1297599240985564 | 29475 |
| map | 0.05123428021631201 | 55324 |
| shop | 0.11631583623546218 | 14377 |
| campfire | 0.04752027624791606 | 24721 |
| other | 0.028550835953097785 | 155665 |

Counts sum1048576 and exactly match update927. Fresh training completions1516/mean floor27.11873350923483; midstart1570/mean32.26433121019108, reported separately. Finitetrue, round optimizer steps64, AMP skips0, episode faults0, stopnull; rank0 pools256/256/256, value coefficient181.3911719048255. Preclip norm mean/max0.7161947693675756/1.519187092781067, clipped rank-attempt fraction0.703125, logged under the existing threshold.

The [52h per-game Act2 ledger](e4-act2-ledger-52h.json) is the exact1428579-byte worker JSON, SHA-256e828947f69bf7964638e438a01321b5985ffb0edb288c6fce9304397bbea413e. All1024 seed/floor/outcome records match the8 standard episode files and8 ledger shards. There are841 Act2-entry snapshots; independently recomputed tied Spearman: Act1Boss marker952 cases/0.5212657923526932, Act2-entry marker841 cases/0.48689771536868276. These condition on reaching the marker. Requested deck IDs/upgrades, relics/potion slots/HP/maxHP/gold, death room ID/encounter roster/entryHP/nonempty potions/turn, explicit skips/net removals/rests/upgrades/elites/stalls are retained; encounter rosters do not identify an individual killer and equal-size transforms do not count as removals.

Ledger outcomes{"death":1020,"stalled":4}; counts{"card_skips":1488,"card_removals":1778,"rests":3344,"upgrades":1690}; elites by act{"1":1137,"2":711,"3":104,"4":0}; Act2-entry means{"deck_size":21.711058263971463,"hp":58.47919143876338,"max_hp":67.5731272294887,"gold":186.54577883472058}. Retained private dev-52.pt SHA-256ceedf560f7a671849ae73e43448e8c770d2e6ed7276475d9b6385bdcf2abae6e; no model binary is published.

#### 54h student telemetry and actual ledger

| Decision type | Normalized entropy | Count |
| --- | ---: | ---: |
| combat | 0.25982952699809025 | 783734 |
| card_reward | 0.13666889782153097 | 27899 |
| map | 0.05765120260096186 | 51884 |
| shop | 0.13948744981388947 | 14161 |
| campfire | 0.04694622234113374 | 24753 |
| other | 0.029253145098331938 | 146145 |

Counts sum1048576 and exactly match update996. Fresh training completions1391/mean floor28.40762041696621; midstart1415/mean32.67491166077738, reported separately. Finitetrue, round optimizer steps64, AMP skips0, episode faults0, stopnull; rank0 pools256/256/256, value coefficient181.3911719048255. Preclip norm mean/max1.9437945513054729/3.485978841781616, clipped rank-attempt fraction1, logged under the existing threshold.

The [54h per-game Act2 ledger](e4-act2-ledger-54h.json) is the exact1414360-byte worker JSON, SHA-2561ee5f8c07a04d0804f241f53262f041ed5ed99e7c8abad55f07b9b9de00e21f7. All1024 seed/floor/outcome records match the8 standard episode files and8 ledger shards. There are855 Act2-entry snapshots; independently recomputed tied Spearman: Act1Boss marker956 cases/0.42793200980836615, Act2-entry marker855 cases/0.4457492500934373. These condition on reaching the marker. Requested deck IDs/upgrades, relics/potion slots/HP/maxHP/gold, death room ID/encounter roster/entryHP/nonempty potions/turn, explicit skips/net removals/rests/upgrades/elites/stalls are retained; encounter rosters do not identify an individual killer and equal-size transforms do not count as removals.

Ledger outcomes{"death":1022,"stalled":2}; counts{"card_skips":3274,"card_removals":2119,"rests":3554,"upgrades":1933}; elites by act{"1":1436,"2":819,"3":188,"4":0}; Act2-entry means{"deck_size":20.79766081871345,"hp":57.839766081871346,"max_hp":67.61286549707603,"gold":194.68187134502924}. Retained private dev-54.pt SHA-2568ba9d60bbc09ec74e2385133bab7af94ecee988707020095938ab90c8b0f9fa1; no model binary is published.

| New LR/KL window | Training seconds (exclusive start, inclusive end) | Committed PPO updates | LR min/max/current/next | KL min/max/end | At1e-5 |
| --- | --- | --- | --- | --- | ---: |
| 46→48h | (165650.30814382993, 172885.2867859129] | 716–788 (73) | 0.0001/0.0001/0.0001/0.0001 | 0.006456824894450926/0.015389569807845163/0.010832943176090074 | 0 |
| 48→50h | (172885.2867859129, 180031.14018304192] | 789–858 (70) | 0.0001/0.0001/0.0001/0.0001 | 0.0066792624904068254/0.015697351027070244/0.010453098841854205 | 0 |
| 50→52h | (180031.14018304192, 187208.44305409092] | 859–927 (69) | 0.0001/0.0001/0.0001/0.0001 | 0.005129461706980146/0.017530592892394253/0.01041971964905423 | 0 |
| 52→54h | (187208.44305409092, 194421.85996289173] | 928–996 (69) | 0.0001/0.0001/0.0001/0.0001 | 0.005428578636180496/0.015702400796041075/0.00618784881041326 | 0 |

Source segments remain per job and phase:46→48h uses4439 updates716–788;48→50h uses4439 committed789–852 plus4441 actual853–858;50→52h uses4441 updates859–927;52→54h uses4441 committed928–985 plus4442 actual986–996. Uncommitted partial branches are not duplicate LR rows; older windows remain closed.

No GPU lookahead,512-model training, new distillation or learner optimization was launched. Existing source40c0f6f2 and the completed value ramp remain unchanged; Next evaluation56h (201600 cumulative training seconds), with LR/KL strictly after194421.85996289173; wait for the existing fairness dependencies to complete.
