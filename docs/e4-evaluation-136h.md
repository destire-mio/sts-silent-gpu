# E4 136h ordinary evaluation

The natural 1,024-seed argmax exam completed in job 5434 on two V100s,
using the 768×12 pre-LN dev-136 checkpoint from PPO update 91
(`489672.9309714598` training seconds).
Mean floor was **30.006836**, down from 30.747070 at 134h and
31.328125 at 132h. Act 1 Boss passes fell from 847 to 820 since
134h. Act 2 Boss passed 236 times and Act 3 first Boss 72
times. Act 3 second-Boss wins were 14 and Heart wins were zero.
The ledger has 1006 deaths,
14 Act 3 wins, and
4 stalled cases, with zero hierarchy
contradictions. Ruby, emerald, and sapphire counts were 34, 195, and 9.
Three runs collected all three keys, one entered Act 4, and one beat Shield and Spear.

| Baseline | Paired floor delta | SE | Seeds |
|---|---:|---:|---:|
| dev-134 | -0.740234 | 0.394711 | 1024 |
| dev-132 | -1.321289 | 0.425309 | 1024 |
| dev-112 | -1.955078 | 0.446455 | 1024 |
| dev-70 | +0.657227 | 0.433610 | 1024 |

The server result continues the decline since dev-132. This is the server's
128-step PPO series. The #184 Colab baseline used 64 steps. Update 91
sampling entropy by decision type was card_reward 0.12809, map 0.06848, shop 0.11660, campfire 0.05036. The server training
configuration stays in effect.

## Strict training window

The window is (482564.2129658388, 489672.9309714598]. All rows used
two GPUs, 1,024 lanes per GPU, rollout 128, global minibatch 32,768 and
microbatch 256. There were zero AMP skips and zero training faults.

| Job | Completed rows | Rollout decisions | Effective rollout rows | Applied in rows | Applied on job |
|---|---|---:|---:|---:|---:|
| 5433 | 77–86 | 2,621,440 | 2,621,431 | 160 | 174 |
| 5434 | 87–91 | 1,310,720 | 1,310,716 | 80 | 66 |

Update 87's 16 applied steps include 14 saved on job 5433. Job 5434
applied the remaining two. The window totals 240 applied steps. Adam at
the lower bound is inferred as 5,111, read as 5,285 at the 5434 resume,
and inferred as 5,351 at the upper bound. Effective rows are the logged
learner sample counts divided by the two PPO epochs.

## Evidence

An independent CPU process recomputed all 1,024 seed pairs, verified the
ledger shards, and strictly loaded one finite model of 90,402,050
parameters. Process exit 0, 5.136s, empty stderr.
Checkpoint SHA256 `6e67e04faf55aa0409a2e0de2703543db167b56a658fccb236b62888c05582d7`, ledger SHA256 `a54239a49a94f12cfff2dd762af956667d7ee2fd7d4be0106fae981402a52dbb`.
Audit SHA256 `714bbe01f0e9239a6f36e7915ae1a6cda3f436a6d502e9b004e0c6ab2a721b9f`, process receipt SHA256 `af81e3a15e1b1abd956785a7155f3176d2854967632bab7352a333fce0bdb6c6`.
