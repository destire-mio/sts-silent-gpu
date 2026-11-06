# E4 98h ordinary evaluation — pure PPO, 学习率封顶 4.44e-5

The retained **dev-98** snapshot completed its natural **1,024-game argmax** exam at update **2307**, training time **352913.22257588117 seconds**, on **8 V100 GPUs**. Mean floor is **30.8740234375**, with **13 Act3 second-boss flags** and **0 Heart wins**. The paired difference versus dev-96 is **+0.259765625**, SE **0.357285261909008**; versus dev-70 it is **+1.5244140625**, SE **0.41232624774944904**. These fixed-seed observations establish no causal effect of LR, engine, GPU count, rollback or pausing CE.

This ordinary exam and its audit/publication are CLOSED. There is no 98h128-seed supplement. Next ordinary threshold: **100h /360000 seconds**, with the next LR window strictly after **352913.22257588117**. The next eight-hour supplement is **104h /374400**, argmax/combat only under #158. Closed96 and earlier audits and seeds were not rerun.

## Protocol, source and model

- Fixed seeds **2000000000..2000001023**, all1024 unique, exact result and ledger shard ownership `seeds[rank::8]`.
- Ordinary argmax, no SL/search, observation version14, `dev78_rollback_pure_PPO`.
- Actual exam worker **5341**, source **47889615f840e169a25c0c1d72f5cf6cb79ce016**, `c173-preparation/runtime`; build `/home/zhuyouwei/spire-build-e4-c157-run-determinize`.
- #153 engine fix and #154 cap remain active. #157 whole-run determinization remains limited to card previews; this ordinary exam does not compare previews. #158 changes only the future supplement schedule.
- **#159 is not active in this exam.** Its future source a721/c175 is bound for a natural successor, while worker5341 remains478896/c173. The first effective update must come from ordinary successor logs. This98h exam does not count toward #160's post-159 three-exam condition. #160 remains CPU preparation only; no large-model switch occurred.
- `learning_rate_cap=4.444444444444445e-5`, **学习率封顶 4.44e-5**.
- Model-only `dev-98.pt`: **49762853 bytes**, SHA-256 `8942e174f2cc148505969b2edbcdd145628e3f8fe2e5f6661aa79e6d1320cf87`.
- Independent CPU strict load verified CLS384 /6 layers /6 heads /1536 feedforward, **12,435,074 parameters**, all tensors finite. The model was loaded once in the first audit process; the corrected continuation uses that completed evidence, as detailed below. No large current rank checkpoint was read.

## Full-game outcomes

| Metric | Count /1024 | Rate |
| --- | --- | --- |
| Act1 boss passed | 863 | 0.8427734375 |
| Act2 boss passed | 226 | 0.220703125 |
| Act3 first boss passed | 49 | 0.0478515625 |
| Act3 second boss flag | 13 | 0.0126953125 |
| Act4 entered | 1 | 0.0009765625 |
| Shield/Spear passed | 0 | 0 |
| Heart victory | 0 | 0 |

Ruby /emerald /sapphire: **57 /175 /41**; all-three keys **6**. Independent key/reach and boss-hierarchy checks found **0 contradictions**.

Seed **2000000749** entered Act4 with all three keys and ended at floor55, Shield/Spear=false and Heart=false.

Ledger labels are **1010 death,13 act3_win,1 stalled**. All1024 games, including the stalled case, enter means and paired comparisons. The existing collector marks terminal episodes that passed the Act3 second boss without Heart as `act3_win` and removes death metadata. These flags are not full-game victories or an exhaustive count of post-Act3 deaths.

| Fixed-seed baseline | Mean floor difference | Paired SE | Pairs |
| --- | --- | --- | --- |
| dev-96 | +0.259765625 | 0.357285261909008 | 1024 |
| dev-70 | +1.5244140625 | 0.41232624774944904 | 1024 |
| dev-16 teacher argmax | +13.404296875 | 0.37145067656302005 | 1024 |

## Act2 ledger

Consolidated `evaluation-2307-act2-ledger.json`: **1670190 bytes**, SHA-256 `b87f89b0f4dadd65cd28c10686970bf4dd401b6aacabef261872fb897dcc0ae3`;1024 episodes and **863 Act2 entry snapshots**. Eight result shards and eight ledger shards agree on seed ownership, floors, keys/reach, normalized outcomes, stalled/death fields, entry decks/relics/potions/HP/gold and counters.

- Card skips /removals /rests /upgrades: **2445 /2609 /3277 /2288**.
- Elites by Act1 /2 /3 /4: **1076 /765 /238 /1**.
- Mean Act2 entry deck size /HP /max HP /gold: **20.852838933951332 /58.53765932792584 /67.57705677867902 /180.00811123986094**.
- Tied-rank Spearman versus final floor: Act1 boss value **0.449227803697045 (967 cases)**; Act2 entry value **0.45933458000529315 (863 cases)**.

## Retained training window and learning rate

Strict window **(345695.23689094814,352913.22257588117]** contains **56 complete pure PPO updates2252..2307**, all read individually from their retained own-job logs. They are finite with beta0/samples0/collectionfalse. Both source phases use the verified #157 build; neither has #159 training midstart whole-run randomization.

| Retained job | GPUs | Runtime source | Updates | Rows | Complete applied steps | AMP skips |
| --- | --- | --- | --- | --- | --- | --- |
| 5322 | 7 | 05fec51 | 2252–2255 | 4 | 224 | 0 |
| 5339 | 8 | 05fec51 | 2256–2272 | 17 | 1088 | 0 |
| 5340 | 7 | 4788961 | 2273–2274 | 2 | 112 | 0 |
| 5341 | 8 | 4788961 | 2275–2307 | 33 | 2111 | 1 |

Update **2282** actually records **63 applied steps and1 AMP skip**; other8-GPU rows record64 and the six7-GPU rows56. Thus complete applied steps are **3535**, not3536. Add the **46 already-applied retained partial steps** at identity **2274 /serial250**, counted in98 exactly once: **3535 +46 =3581 applied steps**. The AMP skip at2282 is separate from the single episode fault at2288.

Current and next LR remain **4.444444444444445e-5** throughout, with **0 lower-bound touches**. KL min **0.007541731907413521**, max **0.01124793329103771**, ending **0.00782619551236111**. The existing KL>.02 division by1.5 and1e-5 floor remain unchanged.

| Job | KL min | KL max | KL end |
| --- | --- | --- | --- |
| 5322 | 0.007541731907413521 | 0.010233297020338735 | 0.010233297020338735 |
| 5339 | 0.007600739156385254 | 0.010509414181668958 | 0.009571338207071149 |
| 5340 | 0.008302581327273609 | 0.0087433052727213 | 0.008302581327273609 |
| 5341 | 0.007820301857201527 | 0.01124793329103771 | 0.00782619551236111 |

### Persisted counters and one-time partial accounting

| Natural successor resume | Input checkpoint /serial | Adam | Retained partial |
| --- | --- | --- | --- |
| 5339 | 2255 /247 | 125502 | 0 |
| 5340 | 2272 /249 | 126590 | 0 |
| 5341 | 2274 /250 | 126748 | 46 |

All three small canonical resumes have rank match=true and new cursor0. World changes were7→8 at5339,8→7 at5340 and7→8 at5341.

- 2252..2255 apply224 steps. Real2255/247 Adam125502 minus224 cross-confirms **96 endpoint Adam125278**, still not a direct saved2251 counter read.
- Real2272/249 Adam126590 minus125502 equals1088, matching2256..2272.
- Real2274/250 Adam126748 minus126590 equals158 =112 from2273/2274 plus retained46.
- 2275..2307 apply2111 steps, including2282's skip. **126748 +2111 =128859**, also **125278 +3581 =128859**.

**Adam128859 at2307 is derived, not a savedcounter read.** Last real counter remains2274/250/126748. Natural periodic saves2269/2288/2302 and allocation-end saves2255/2272/2274 add no optimizer work.

The already-closed96partial0,94partial81 (2141/237:3,2144/238:36,2152/239:42),92partial8 (2132/236),90partial89 (2051/227:12,2058/229:34,2069/231:30,2072/232:13) and earlier partials are excluded from new accounting. The new46 belongs only to98 and must not be added in100 or later. Historical orphan branches remain excluded.

### Students and the previously reported episode fault

Nominal student decisions **57,933,824 = Σ(N×2048×64)**; valid mainline **57,933,823**, difference **1**. Per-type decision counts include attempted fault actions; valid-mainline counts exclude them.

The already-notified fault occurs in5341 after2287: seed **1741093629**, `engine_fault:852769260`, parent/slot418, probe=null, start_kind=fresh,1516 candidates, randomizations=[]. Update2288 remains finite64-step/0-AMP but has valid1048575 versus nominal1048576 and fault1;2289 returns to1048576/fault0. This is not a win or a terminal allocation failure. Its cause is unverified; it is not relabeled as #155's different capacity fault or as recurrence of an older assertion.

The exact raw [fault record](faults/episode-fault5341-seed1741093629-after2287.json) is **4754 bytes**, SHA-256 `f308f82bc75e6839886d7c02813513ede0505e44aa28c76dfc420553dea25a36`. The original c174 local/remote bytes were reused, with no replay or repeat fault notification.

At2307 the valid/nominal count is1048576. Counts: combat792491,card_reward25910,map50537,shop15101,campfire24892,other139645. One observed fault is retained; this does not establish long-term fault absence.

## Final five expert-round fields

Updates2303..2307 each explicitly log beta0,samples0,collectionfalse. **N/A is an absent measurement while CE/expert collection is paused, not measured zero.** The70h field scope is retained.

| Update | GPUs | L | Next L | Samples | Samples/s | Ticks | Overhead | CE | Argmax agreement | Allocated peak | Projected allocation | Measured bytes/lane | Mem limit event | OOM recovered event |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2303 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2304 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2305 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2306 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |
| 2307 | 8 | N/A | N/A | 0 | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | None | None |

Neither `expert_lane_memory_limit` nor `expert_oom_recovered` occurs in these five intervals. Allocation probes/job failures are separate from expert recovery events.

| Update | v2 override labels | CE grad norm | PPO grad norm for CE merge | CE scale | Combat entropy |
| --- | --- | --- | --- | --- | --- |
| 2303 | N/A | N/A | N/A | N/A | 0.3296813963484832 |
| 2304 | N/A | N/A | N/A | N/A | 0.331954686389086 |
| 2305 | N/A | N/A | N/A | N/A | 0.33164691526213824 |
| 2306 | N/A | N/A | N/A | N/A | 0.32940279884127904 |
| 2307 | N/A | N/A | N/A | N/A | 0.32863696767770667 |

Ordinary PPO/AMP/clipping continues while CE is paused:

| Update | PPO norm mean | PPO norm max | Clipped fraction | AMP scale | AMP skips | Normalized entropy |
| --- | --- | --- | --- | --- | --- | --- |
| 2303 | 1.071725065819919 | 1.9365673065185547 | 1 | 131072 | 0 | 0.26064396183899596 |
| 2304 | 0.8281124709174037 | 1.1028603315353394 | 1 | 131072 | 0 | 0.2623108086763337 |
| 2305 | 0.7970716208219528 | 1.074244499206543 | 1 | 131072 | 0 | 0.263457583954682 |
| 2306 | 0.9710805779322982 | 1.3213871717453003 | 1 | 131072 | 0 | 0.2616143347014541 |
| 2307 | 1.25925328489393 | 1.8743709325790405 | 1 | 131072 | 0 | 0.25881761005002835 |

These are the logged PPO norm statistics, not CE-merge measurements. Minibatch means of gradient norms and CE scales must not be multiplied to assert a cap.

| Update | GPUs | Per-rank RSS peaks, bytes |
| --- | --- | --- |
| 2303 | 8 | `2507268096, 2373033984, 2399506432, 2378137600, 2362343424, 2365128704, 2376114176, 2411048960` |
| 2304 | 8 | `2507268096, 2373033984, 2399506432, 2378137600, 2362343424, 2365128704, 2376114176, 2411048960` |
| 2305 | 8 | `2507268096, 2373033984, 2399506432, 2378137600, 2362343424, 2365128704, 2376114176, 2411048960` |
| 2306 | 8 | `2507268096, 2373033984, 2399506432, 2378137600, 2362343424, 2365128704, 2376114176, 2411048960` |
| 2307 | 8 | `2507268096, 2373033984, 2399506432, 2378137600, 2362343424, 2365128704, 2376114176, 2411048960` |

RSS peaks are unchanged across these five observations on the same8-GPU worker. This short window proves neither long-term growth nor its absence.

## Audit execution and provenance

The audit used existing retained results, model, ledger and own-job logs on CPU. **Two audit processes actually ran**, with the failed first process preserved:

1. First process: **4.621539115905762 seconds**, exit1. It completed strict model loading/finite/parameter-count checks, shard ownership, ledger structure/outcomes, paired computations and trajectory reads before a hard-coded3536-step/zero-skip expectation failed. The real unread2282 row has63 steps/1 skip; this was an audit assumption error, not a model or worker failure.
2. Corrected continuation: **3.0906758308410645 seconds**, exit0, empty stderr. It uses the first process's completed strict-model evidence and **does not reload model weights**; it reconstructs the final audit with the actual3535 complete steps/1 skip and46 partial steps. The failed script and execution receipt remain alongside the corrected ones. This is not described as a single-process audit.

No GPU evaluation/test, seed rerun, engine change, new marker, collector clear, forced save, worker signal, extra resource query or current large rank read was performed.

- Audit: `c177-preparation/evaluation98.audit.original.json`, **204324 bytes**, SHA-256 `fbefdbb402dc0385dfde2e9b5f31170af042eff6322fa423a5abbd1b6808310f`.
- Completed script SHA-256 `02474b2335133a8b0a24b061b8284175f6b85e95cac6bc8f0c17c8fb8f5553b8`.
- Initial script SHA-256 `2787c59666b8ab3f3e14bb11f6c65d45a20d21297c9d82ed12770e42044acae5`.
- Execution receipts and both scripts are preserved exactly locally and remotely; no older closed audit was rerun.

| Artifact | Bytes | SHA-256 |
| --- | --- | --- |
| evaluation-2307-rank-0.json.gz | 1281 | `c0b73ef9f8b1c0881f83cc2f9be19fd359271f31e9b1dbb2a9eeec111b4c572d` |
| evaluation-2307-ledger-rank-0.json.gz | 14676 | `8f6cc51e56966dd18ca864ac0e4cdecf24e7972876dd67a7bdf747da47d31d6a` |
| evaluation-2307-rank-1.json.gz | 1327 | `1c0c5bf59b73f22e3672360812ca81aae3c2d342ad58ffd00f4ff502653a6708` |
| evaluation-2307-ledger-rank-1.json.gz | 14671 | `83b78f34ee2c830185c76cb994c813d72bc2ea4e3fe43d882d9f28281fab3c6a` |
| evaluation-2307-rank-2.json.gz | 1322 | `91fe35c8b03634ec17c0d2ec6e633c9fc75a821c819d392b901f31e6ee254e9c` |
| evaluation-2307-ledger-rank-2.json.gz | 14382 | `ca1b006924a3566ed9c91e50f509ca99cfa946c9f190b03c04de7cb8e76c5f3f` |
| evaluation-2307-rank-3.json.gz | 1299 | `db32790f518835edfa02870cf35585a0db7b307f66b4ce36f989735c707c8823` |
| evaluation-2307-ledger-rank-3.json.gz | 14631 | `9574fbb5d694fb94ebb0357baf8560c809b8ee419591407e6bb58f324ffdd097` |
| evaluation-2307-rank-4.json.gz | 1252 | `8d1ecb15a4fdc3b22e88d918e6b397b8cf24343c4cf524373601c2a6e428c70b` |
| evaluation-2307-ledger-rank-4.json.gz | 14759 | `bd15f0bbcfa9c691d2b2653c6d2cd35ab15a3d99b9d9afeaeddbedd969c7b2d8` |
| evaluation-2307-rank-5.json.gz | 1319 | `8f8ce0f1011f715d120531c65298796a2056b8e5d057f4a1e7fbdbcaead0ff6e` |
| evaluation-2307-ledger-rank-5.json.gz | 15048 | `a5be3acecb33ed9eaabdac08d82f04e97c758a2ddedd4dd1edc00c6275713d8e` |
| evaluation-2307-rank-6.json.gz | 1317 | `f5f5ddad93451d060900015fac1577d0d832f880f7b9fb5af7aedc8d0d51fe07` |
| evaluation-2307-ledger-rank-6.json.gz | 15249 | `7d88569886fcec39d880f948208b7a7a1adda42447001f479994d23e65531ae1` |
| evaluation-2307-rank-7.json.gz | 1327 | `9910b0b4f525aca0d589dda12acfd818edabfb71891fd2ef54e4deb45ab053f5` |
| evaluation-2307-ledger-rank-7.json.gz | 15194 | `c6f06b3e0ca19148e4da32d9676c03daf03bc071cfe0d0f7f33599cd1fe73c19` |

Own-job source-log identities at audit read time:

| Job | Log | Bytes | SHA-256 |
| --- | --- | --- | --- |
| 5322 | job-5322.log | 341150 | `9af33150a44fb08ee983c4b272032d31c84f3e216779012ba93a9407c6206cf5` |
| 5339 | job-5339.log | 73104 | `ed9fc8b2868496181abb069ba714d9ad8cb88d9bdcd76aef1349f3e2e8c41353` |
| 5340 | job-5340.log | 24002 | `a70766b6278eb32c96610f06a02f2b6fca92eb4bb19644d2f63a5e1bd71e6fb3` |
| 5341 | job-5341.log | 170499 | `45d6c8c80d1e72f1a8501ecb983e863e7f32e77528737bf53000694e7fd82ac4` |

The active5341 log can grow; its audit-time hash does not promise an immutable live file.

Ordinary98 is CLOSED. Continue natural training and ordinary small-log observation, without adding2282's skipped step or any old partial a second time. First actual #159 update is still pending. Next100h window starts strictly after352913.22257588117;104h supplement remains argmax/combat only. Large-model preparation #160 does not authorize a switch.
