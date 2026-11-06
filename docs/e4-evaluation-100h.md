# E4 100h ordinary evaluation — pure PPO, 学习率封顶 4.44e-5

The retained **dev-100** snapshot completed its natural **1,024-game argmax** exam at update **2362**, training time **360098.7655653534 seconds**, on **8 V100 GPUs**. Mean floor is **30.6416015625**, with **11 Act3 second-boss flags** and **0 Heart wins**. The paired difference versus dev-98 is **−0.232421875**, SE **0.37597711418779584**; versus dev-70 it is **+1.2919921875**, SE **0.40415408653107093**. These fixed-seed observations establish no causal effect of LR, engine, GPU count, rollback or pausing CE.

This ordinary exam and its audit/publication are CLOSED. There is no 100h128-seed supplement. Next ordinary threshold: **102h /367200 seconds**, with the next LR window strictly after **360098.7655653534**. The next eight-hour supplement is **104h /374400**, argmax/combat only under #158. Closed98 and earlier audits and seeds were not rerun.

## Protocol, source and model

- Fixed seeds **2000000000..2000001023**, all1024 unique, exact result and ledger shard ownership `seeds[rank::8]`.
- Ordinary argmax, no SL/search, observation version14, `dev78_rollback_pure_PPO`.
- Actual exam worker **5341**, source **47889615f840e169a25c0c1d72f5cf6cb79ce016**, `c173-preparation/runtime`; build `/home/zhuyouwei/spire-build-e4-c157-run-determinize`.
- #153 engine fix and #154 cap remain active. #157 whole-run determinization remains limited to card previews; this ordinary exam does not compare previews. #158 changes only the future supplement schedule.
- **#159 is not active in this exam.** Its future source a721/c175 is bound for a natural successor, while worker5341 remains478896/c173. The first effective update must come from ordinary successor logs. This100h exam, like98h, does not count toward #160's post-159 three-exam condition. Claude decides switch timing; no automatic trigger or large-model switch occurred.
- `learning_rate_cap=4.444444444444445e-5`, **学习率封顶 4.44e-5**.
- Model-only `dev-100.pt`: **49768265 bytes**, SHA-256 `5a87cf61a3e44e0270d982c7564eb3fe7a53aa01133c6ab0574f714393ed249e`.
- Independent CPU strict load verified CLS384 /6 layers /6 heads /1536 feedforward, **12,435,074 parameters**, all tensors finite. No current large rank checkpoint was read.

## Full-game outcomes

| Metric | Count /1024 | Rate |
| --- | --- | --- |
| Act1 boss passed | 868 | 0.84765625 |
| Act2 boss passed | 231 | 0.2255859375 |
| Act3 first boss passed | 55 | 0.0537109375 |
| Act3 second boss flag | 11 | 0.0107421875 |
| Act4 entered | 2 | 0.001953125 |
| Shield/Spear passed | 1 | 0.0009765625 |
| Heart victory | 0 | 0 |

Ruby /emerald /sapphire: **71 /163 /56**; all-three keys **9**. Independent key/reach and boss-hierarchy checks found **0 contradictions**.

| Act4 seed | Final floor | All-three keys | Shield/Spear | Heart |
| --- | --- | --- | --- | --- |
| 2000000617 | 56 | true | true | false |
| 2000000183 | 55 | true | false | false |

Ledger labels are **1011 death,11 act3_win,2 stalled**. All1024 games, including both stalled cases, enter means and paired comparisons. The existing collector marks terminal episodes that passed the Act3 second boss without Heart as `act3_win` and removes death metadata. These flags are not full-game victories or an exhaustive count of post-Act3 deaths.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
| --- | --- | --- | --- |
| dev-98 | -0.232421875 | 0.37597711418779584 | 1024 |
| dev-70 | +1.2919921875 | 0.40415408653107093 | 1024 |
| dev-16 teacher argmax | +13.171875 | 0.37370682400420113 | 1024 |

## Act2 ledger

Consolidated `evaluation-2362-act2-ledger.json`: **1672834 bytes**, SHA-256 `93c874dc98c3d8c54b9858edd28b8eda5499270ce74e34363d04d45332b65f42`;1024 episodes and **868 Act2 entry snapshots**. Eight result shards and eight ledger shards agree on seed ownership, floors, keys/reach, normalized outcomes, stalled/death fields, entry decks/relics/potions/HP/gold and counters.

- Card skips /removals /rests /upgrades: **2012 /2592 /3309 /2187**.
- Elites by Act1 /2 /3 /4: **1123 /762 /237 /2**.
- Mean Act2 entry deck size /HP /max HP /gold: **20.76958525345622 /58.43663594470046 /67.70391705069125 /180.75230414746545**.
- Tied-rank Spearman versus final floor: Act1 boss value **0.44512669499944635 (961 cases)**; Act2 entry value **0.4387810094782655 (868 cases)**.

## Retained training window and learning rate

Strict window **(352913.22257588117,360098.7655653534]** contains **55 complete pure PPO updates2308..2362**, all read individually from the retained own-job log. All55 are finite, world8, beta0/samples0/collectionfalse, source478896/c173, #157 build, before #159 training midstart whole-run randomization.

| Retained job | GPUs | Runtime source | Updates | Rows | Complete applied steps | AMP skips |
| --- | --- | --- | --- | --- | --- | --- |
| 5341 | 8 | 47889615f840e169a25c0c1d72f5cf6cb79ce016 | 2308–2362 | 55 | 3519 | 1 |

Update **2335** actually records **63 applied steps and1 AMP skip**; the other54 updates record64. Complete applied steps are **3519**, plus **new retained partial0**, total **3519**. The count comes from every actual row, without assuming an unobserved round's steps or skips. There is no change of allocation/world/source inside this window.

Current and next LR remain **4.444444444444445e-5** throughout, with **0 lower-bound touches**. KL min **0.007920794641027795**, max **0.011246541642404795**, ending **0.009639405939190451**. The existing KL>.02 division by1.5 and1e-5 floor remain unchanged.

### Counters and partial deduplication

The closed98 audit derives endpoint Adam **128859** from the real2274/250/Adam126748 resume plus2111 applied steps through2307. It is **not a saved2307 counter read**. This100 audit uses the exact closed98 receipt as its anchor; it does not rerun the98 model, ledger or training-window audit.

There is no new successor resume within100 and no new partial to add. **128859 +3519 =132378** is the derived Adam at2362, **not a savedcounter read**. The last real small canonical counter remains **2274 /serial250 /Adam126748**. `dev-100.pt` contains model weights, not Adam. Natural periodic saves2316/2330/2344/2358 add no optimizer work; their new serial/Adam/cursor was not read here.

**The46 retained partial steps at2274/250 were already counted in98 and are excluded from100.** The closed96partial0,94partial81,92partial8,90partial89 and all earlier partials also remain excluded. Historical orphan branches remain excluded. The next102 window starts strictly after360098.7655653534.

### Students and faults

Nominal and valid mainline student decisions both equal **57,671,680 =55×8×2048×64**. Every row's decision-type sum and valid count was checked. The window records **0 episode faults**, which does not establish long-term fault absence. The previous2288 one-action gap and already-notified raw fault belong to98 and are not counted or notified again.

At2362 counts are combat788833,card_reward26729,map51444,shop15360,campfire24478,other141732, summing to1048576. Per-type entropy values are retained below:

| Decision type | Decisions | Entropy |
| --- | --- | --- |
| combat | 788833 | 0.3392266501497845 |
| card_reward | 26729 | 0.14242257592629587 |
| map | 51444 | 0.05370416442853054 |
| shop | 15360 | 0.11798518343936955 |
| campfire | 24478 | 0.04739552183644731 |
| other | 141732 | 0.026586323597645466 |

## Final five expert-round fields

Updates2358..2362 each explicitly log beta0,samples0,collectionfalse. **N/A is an absent measurement while CE/expert collection is paused, not measured zero.** The70h field scope is retained.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocation | Measured bytes/lane | Mem limit event | OOM recovered event |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2358 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2359 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2360 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2361 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2362 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurs in these five intervals. Allocation probes/job failures are separate from expert recovery events.

| Update | v2 override labels | CE grad norm | PPO grad norm for CE merge | CE scale | Combat entropy |
| --- | --- | --- | --- | --- | --- |
| 2358 | N/A | N/A | N/A | N/A | 0.34294535068584 |
| 2359 | N/A | N/A | N/A | N/A | 0.3402662059923803 |
| 2360 | N/A | N/A | N/A | N/A | 0.3378779888746545 |
| 2361 | N/A | N/A | N/A | N/A | 0.33734025993937455 |
| 2362 | N/A | N/A | N/A | N/A | 0.3392266501497845 |

Ordinary PPO/AMP/clipping continues while CE is paused:

| Update | PPO norm mean | PPO norm max | Clipped fraction | AMP scale | AMP skips | Normalized entropy |
| --- | --- | --- | --- | --- | --- | --- |
| 2358 | 0.8045628992840648 | 1.0273349285125732 | 1 | 131072 | 0 | 0.27053697101528096 |
| 2359 | 0.8202906800433993 | 1.0357880592346191 | 1 | 131072 | 0 | 0.2686368604704574 |
| 2360 | 1.157163674943149 | 1.9121652841567993 | 1 | 131072 | 0 | 0.26608543742258556 |
| 2361 | 1.1137516433373094 | 1.6380691528320312 | 1 | 131072 | 0 | 0.2640520568484657 |
| 2362 | 0.8692606585100293 | 1.801080584526062 | 1 | 131072 | 0 | 0.2678069623317091 |

These are the logged PPO norm statistics, not CE-merge measurements. Minibatch means of gradient norms and CE scales must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
| --- | --- | --- |
| 2358 | 8 | `2519896064, 2374868992, 2401341440, 2380029952, 2363351040, 2366963712, 2377031680, 2413461504` |
| 2359 | 8 | `2533019648, 2374868992, 2401341440, 2380029952, 2364268544, 2366963712, 2377031680, 2413461504` |
| 2360 | 8 | `2533019648, 2374868992, 2401341440, 2380029952, 2364268544, 2366963712, 2377031680, 2413461504` |
| 2361 | 8 | `2533019648, 2374868992, 2401341440, 2380029952, 2364268544, 2366963712, 2377031680, 2413461504` |
| 2362 | 8 | `2533019648, 2374868992, 2401341440, 2380029952, 2364268544, 2366963712, 2377031680, 2413461504` |

Rank0 and rank4 peaks increase from2358 to2359, followed by four unchanged observations. This short window proves neither long-term growth nor its absence. Logged2362 CUDA allocated/reserved peaks are5146862080/9193914368 bytes; these are retained training-log values, not a new GPU resource query or a long-term OOM guarantee.

## Audit execution and provenance

The independent CPU audit ran **once**, **4.184746980667114 seconds**, exit0, empty stderr. Strictfinite12435074-parameter model loading,16 result/ledger shards, consolidated ledger, three paired comparisons, all55 actual training rows, partial exclusion and the final five expert rounds passed. The actual2335 skip is retained rather than replaced by an expected zero.

No GPU evaluation/test, seed rerun, engine change, new marker, collector clear, forced save, worker signal, extra resource query, current large rank read or old closed audit was performed.

- Audit: `c178-preparation/evaluation100.audit.original.json`, **197377 bytes**, SHA-256 `fb2dcfa868fd2b684ff7722905b78f1cd6f026d4e13a15360a27b63fbb30283c`.
- Script: `audit100.py`,14431 bytes, SHA-256 `f7c45ed17f0aaf8408630abc026a44c29e3724bdbfec18d948c82b8a48e46e4e`.
- Execution receipt: `audit-process.original.json`,609 bytes, SHA-256 `7d8d11f55924b16d4ec82ca2acf3d3ee77724f21d4307c8701b46a710b06fda7`.
- These three originals are preserved exactly locally and remotely.
- Closed98 anchor receipt:9221 bytes, SHA-256 `4cccae4c65e0d239dc3d56f055c4a9465c41d86f43faabee85ee029a98c3a692`.

| Artifact | Bytes | SHA-256 |
| --- | --- | --- |
| evaluation-2362-rank-0.json.gz | 1300 | `cdaec9afae9998dafe3548abce01a7279bc413ff3658cb5ff97d94f115a9d3bb` |
| evaluation-2362-ledger-rank-0.json.gz | 14255 | `292b48fb96253089d6b65b6f80f7b5ef48e879e0591c37d43448252650e73949` |
| evaluation-2362-rank-1.json.gz | 1382 | `363102c576e8d77b31b03a1ab9b79baa5ce46998209a66eb87978ab441a5a481` |
| evaluation-2362-ledger-rank-1.json.gz | 14970 | `5ab73acd2491075aba207e87a130a395b32aa28910918b7e673dd3e047141325` |
| evaluation-2362-rank-2.json.gz | 1353 | `2f4337283b8511ae7e5f18c8550fd425f46326045ae8a44caa00a4a46af1d4d1` |
| evaluation-2362-ledger-rank-2.json.gz | 14640 | `56eaf7bbeb842e5c01d7c21123337ec1e6e223eb7db32c135a7916290b6253ad` |
| evaluation-2362-rank-3.json.gz | 1291 | `f98a20228a1e9ebd84125ad155bece043b32587bff2bb18c3161dd7e5f1ff95b` |
| evaluation-2362-ledger-rank-3.json.gz | 14802 | `94ba8029b844ee95c27d03437dbc69aa77a97270ddeea5a3d90608e48aa86590` |
| evaluation-2362-rank-4.json.gz | 1282 | `96bc23bd623884622a309c432395a20d6c14f8689bd3315789ac9ec93b8c1737` |
| evaluation-2362-ledger-rank-4.json.gz | 14968 | `4d5daa1d93487fd2861a3eb4a364d5083ab88bae722ed5c7f3ae4d771f8da78f` |
| evaluation-2362-rank-5.json.gz | 1337 | `cd6b33bbf598d486e762f60c2d006b82e002c13c97bb9010fe2de9debd11d7fa` |
| evaluation-2362-ledger-rank-5.json.gz | 15184 | `f9df8069ac268ab37e092054830f8de0c8b223dfdfdda701bc723231989a9615` |
| evaluation-2362-rank-6.json.gz | 1333 | `e8bc1913695c6aa6e1f271a09448b7518d78b73c305fbcc3b36e79ff8e265b50` |
| evaluation-2362-ledger-rank-6.json.gz | 15410 | `751f81a6ba29abc613571cd6f22f1845da9be88073f24b504a67490e0c8d6da3` |
| evaluation-2362-rank-7.json.gz | 1310 | `16f14c28256e0a69f247ea6f75662fba46d03aa4282a78acd2e3675221594447` |
| evaluation-2362-ledger-rank-7.json.gz | 15108 | `aff73974e78e2055cbf95724d18b8184d54a420780ded7ba934d4c4e9de46071` |

Own-job source-log identity at audit read time:

| Job | Log | Bytes | SHA-256 |
| --- | --- | --- | --- |
| 5341 | job-5341.log | 351834 | `14f7699b9dbc04e14bef5750f67f08883aed3fdaf2b3af2d8f401e1e17f843de` |

The active5341 log can grow; its audit-time hash does not promise an immutable live file.

Ordinary100 is CLOSED. Continue natural training and ordinary small-log observation. Next102h367200 has strict LR window>360098.7655653534;104h supplement remains argmax/combat only. #159 actual first effective update remains pending; #160 preparation does not authorize a switch.
