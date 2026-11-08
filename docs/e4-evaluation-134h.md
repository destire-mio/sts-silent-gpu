# E4 134-hour ordinary evaluation

Job 5433 evaluated `dev-134.pt` after PPO update 76 at training clock 482564.2129658388 s. The ordinary threshold was 482400 s. The model is the 768-wide, 12-layer pre-LN student. Evaluation used argmax on 1,024 seeds with two V100s and runtime `5f1b2c2a8b5e48c478fdbe4a3d9e3aca8103dbe0`.

## Results

| Metric | Count or value |
|---|---:|
| Mean floor | 30.7470703125 |
| Act 1 boss | 847 |
| Act 2 boss | 240 |
| Act 3 first boss | 63 |
| Act 3 second boss | 21 |
| Ruby / emerald / sapphire keys | 37 / 198 / 30 |
| All three keys | 8 |
| Act 4 entry / Shield and Spear win / Heart win | 1 / 1 / 0 |
| Ledger death / Act 3 win / stalled | 999 / 21 / 4 |

Paired floor differences use all 1,024 seeds in each comparison.

| Baseline | Mean difference | Paired SE |
|---|---:|---:|
| dev-132 | -0.5810546875 | 0.40664019964507253 |
| dev-112 | -1.21484375 | 0.4363209552220371 |
| dev-70 | +1.3974609375 | 0.43908475871180336 |

## Training window

The evaluation belongs to the **128-step group**, which had collected **524,288** decisions at this snapshot. From dev-132 to the switch after update 74, the server collected **3,145,728** decisions with the 64-step layout. Total PPO decisions since dev-132 were **3,670,016**.

The strict clock window is (475739.6731542035, 482564.2129658388].

| Job | Rollout | Updates | Applied / attempts | Valid decisions |
|---|---:|---:|---:|---:|
| 5432 | 64 | 63–73 | 176 / 176 | 2,883,584 |
| 5433 | 64 | 74 | 16 / 16 | 262,144 |
| 5433 | 128 | 75–76 | 32 / 32 | 524,288 |

Across the window there were zero AMP skips and zero episode faults. The controller read Adam step 4887 at the window start and 5063 at the 5433 resume. Both resumes had zero partial learner steps. The window endpoint is inferred at Adam step 5111. The PPO global minibatch was 32768.

## Independent CPU audit

The audit reconstructed the results from both rank shards and matched all 1,024 ledger entries by seed. It found zero progression contradictions and verified the baseline floors against the original dev-112 shards. One strict CPU load checked all 90,402,050 model parameters for finiteness. The audit exited 0 in 8.139890030957758 s with empty stderr.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| dev-134.pt | 361658543 | `50d403aee7f81ac16117f54bbb6efc223acbaf5280013603f3fd433e68a6c3d1` |
| evaluation-76-act2-ledger.json | 1626971 | `c158988e29493f9f03215b902a17245f0654ece50f05dd466280b8fbfa9366dd` |
| Independent audit | 6458 | `ca59913ecac22386f55ac87bc5949eda2bcd6b629514dcf39f4f07a5c164629c` |
| CPU process record | 120 | `675a013687ca465dbcadb5df70d0b6b88d01e82756b071ed94c4f6aa27aa5216` |
