# E4 118-hour evaluation

Job 5388 completed the regular 1,024-seed argmax evaluation at PPO update **2839**, training clock **424903.54684045917 s**. It ran the 384×6 policy after the #165 rollback to dev-112, with four V100s, runtime `30eb3357d32abbd27b9decbf157c5ddd80d54075` and the #157 engine build. Seeds 2000000000–2000001023 ran without search.

| Outcome | Result |
| --- | ---: |
| Mean floor | 31.078125 |
| Act 1 boss | 859 / 1024 |
| Act 2 boss | 242 / 1024 |
| Act 3 first boss | 75 / 1024 |
| Act 3 second-boss flag | 21 / 1024 |
| Heart | 0 / 1024 |
| Ruby / emerald / sapphire | 86 / 164 / 85 |
| All three keys / Act 4 entries / Shield and Spear wins | 30 / 6 / 3 |
| Ledger death / act3_win / stalled | 1001 / 21 / 2 |

| Paired argmax reference | Mean floor difference | Paired SE | Seeds |
| --- | ---: | ---: | ---: |
| dev-112 | -0.8837890625 | 0.4036736995691652 | 1024 |
| dev-116 student | 22.513671875 | 0.3781734417987984 | 1024 |
| dev-70 | 1.728515625 | 0.4102456050769787 | 1024 |

The stored summary labels its baseline `dev-16.pt argmax`. The audit matched the stored baseline floor to the independently loaded dev-112 floor for every seed. All 1,024 episodes enter every comparison. The audit found zero boss and key progression contradictions. The `act3_win` ledger label records the Act 3 second-boss flag. The comparison with dev-116 spans the rollback from the 768 student to the 384 policy.

## Training interval

The accounting window is **(417746.18399990676, 424903.54684045917]** training seconds.

| Job and phase | Updates | GPUs | Optimizer steps | AMP skips | Nominal actions | Valid actions | Episode faults |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 5387, 768 distillation | 8 | 4 | 32 | 0 | 524,288 | 524,288 | 0 |
| 5388, 384 PPO after rollback | 2784–2839 | 4 | 1,792 | 0 | 29,360,128 | 29,360,127 | 1 |

The 768 checkpoint retained update 8 before the CUDA OOM. The subsequent student update 9 was lost at rollback and is excluded along with its post-update fault. The two rows represent separate optimizer histories. The 384 resume reported Adam 156337 at checkpoint 2783, serial 295. Adding the 1,792 PPO steps gives an inferred endpoint of 158129. The audit used the model snapshot and bounded logs. Earlier retained partial steps of 18 and 46 stay in their original reporting windows.

Current and next LR held at 4.444444444444445e-5, with zero lower-bound touches. In the PPO segment, KL min / max / end was 0.005981665783524243 / 0.014119127142604578 / 0.013779713253948358. The single distillation round logged KL 0.03272092098760693 under its own training phase.

After update 2811, seed 1849642368 ended with `engine_fault:852769260` at slot 1578 in Act 3. The root cause remains unverified. Update 2812 completed 32 optimizer steps, zero AMP skips and 524,287 valid actions. Update 2813 returned to 524,288 valid actions and zero faults. The [original fault record](faults/episode-fault5388-seed1849642368-after2811.json) preserves the decimal bytes of the randomization seeds. Its SHA-256 is `d8eb43b17f03a101ca54a723e4ed30296b9b425279048e9ee3792556be659dfe`.

## Last five PPO updates

Expert beta and samples were zero, with collection disabled for all five updates. Expert lane memory-limit and OOM-recovery events were absent in this interval. Expert loss, scaling, selection and other expert-only metrics are N/A. PPO gradient norms and AMP scale are listed separately.

| Update | PPO gradient norm mean / max | AMP scale | Combat entropy | RSS peaks per rank (bytes) |
| ---: | --- | ---: | ---: | --- |
| 2835 | 1.018594490364194 / 1.4813125133514404 | 131072.0 | 0.28611953118622296 | 2493558784 / 2330976256 / 2347855872 / 2334019584 |
| 2836 | 0.9636757634580135 / 1.2331987619400024 | 131072.0 | 0.28132027258736636 | 2493558784 / 2330976256 / 2347855872 / 2334019584 |
| 2837 | 1.016397474333644 / 1.9137376546859741 | 131072.0 | 0.2827701634738196 | 2493558784 / 2330976256 / 2347855872 / 2334019584 |
| 2838 | 1.2108492702245712 / 1.890363335609436 | 131072.0 | 0.284554785614915 | 2493558784 / 2330976256 / 2347855872 / 2334019584 |
| 2839 | 1.278577834367752 / 2.0052263736724854 | 131072.0 | 0.2815990353846224 | 2493558784 / 2330976256 / 2347855872 / 2334019584 |

## Independent CPU audit

One CPU audit process completed in 4.341997146606445 seconds with exit 0 and empty stderr. One strict CPU model load verified all 12,435,074 parameters were finite. The audit reconciled four result shards, four ledger shards, the combined ledger, the paired references and all 57 complete training rows in the strict window. It reused the natural evaluation outputs.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-118.pt` | 49768265 | `d1f2716d81c761515e4334c232da7eb40686e2d4d9165ad5099eec99e281e283` |
| `evaluation-2839-act2-ledger.json` | 1670968 | `d96149c08220778a033ad82dc8a1da912d34292c74d9468d7287b261a8a7ef94` |
| `evaluation118.audit.original.json` | 192345 | `4ca0f5e2d0197d3a428c753a02a4ab07423928093d96d75dd3c2cbb6bf1c7c31` |
