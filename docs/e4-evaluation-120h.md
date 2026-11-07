# E4 120-hour evaluation

Job 5388 completed the regular 1,024-seed argmax evaluation at PPO update **2899**, training clock **432097.7652507751 s**. It used the 384×6 policy on the dev-112 rollback lineage, four V100s, runtime `30eb3357d32abbd27b9decbf157c5ddd80d54075` and the #157 engine build. Seeds 2000000000–2000001023 ran without search.

| Outcome | Result |
| --- | ---: |
| Mean floor | 29.8271484375 |
| Act 1 boss | 815 / 1024 |
| Act 2 boss | 206 / 1024 |
| Act 3 first boss | 53 / 1024 |
| Act 3 second-boss flag | 8 / 1024 |
| Heart | 0 / 1024 |
| Ruby / emerald / sapphire | 73 / 189 / 49 |
| All three keys / Act 4 entries / Shield and Spear wins | 23 / 3 / 3 |
| Ledger death / act3_win / stalled | 1015 / 8 / 1 |

| Paired argmax reference | Mean floor difference | Paired SE | Seeds |
| --- | ---: | ---: | ---: |
| dev-118 | -1.2509765625 | 0.4073968451887802 | 1024 |
| dev-112 | -2.134765625 | 0.4058790348248843 | 1024 |
| dev-70 | 0.4775390625 | 0.4393110767405117 | 1024 |

The stored summary labels its baseline `dev-16.pt argmax`. The audit matched its floor to the independently loaded dev-112 floor on every seed. All 1,024 episodes enter each comparison. The audit found zero boss or key progression contradictions. The `act3_win` ledger label records the Act 3 second-boss flag.

## Training interval

The strict accounting window is **(424903.54684045917, 432097.7652507751]** training seconds. All 60 completed updates (2840–2899) came from job 5388 on four V100s. They recorded 1,919 applied optimizer steps, one AMP skip at update 2850, 31,457,280 nominal actions and 31,457,278 valid actions. Earlier partial work stays in its original reporting window.

The PPO endpoint Adam count is inferred as 160048 from the rollback resume counter 156337 plus 1792 steps through 118h and 1919 in this window. Current and next LR held at 4.444444444444445e-5, with zero lower-bound touches. KL min / max / end was 0.00710092415543187 / 0.015179658378552114 / 0.01172800374884038.

| Fault after update | Seed | Start | Affected update | Valid actions | Original record |
| ---: | ---: | --- | ---: | ---: | --- |
| 2848 | 1306859274 | Act 2 | 2849 | 524287 | [Raw JSON](faults/episode-fault5388-seed1306859274-after2848.json) |
| 2870 | 1003920883 | Act 2 boss | 2871 | 524287 | [Raw JSON](faults/episode-fault5388-seed1003920883-after2870.json) |

Both episodes logged `engine_fault:852769260`. The root causes remain unverified. The next update after each fault recorded zero episode faults and 524,288 valid actions. The raw files preserve the original decimal randomization seed bytes.

## Last five PPO updates

Expert beta and samples were zero, with collection disabled on all five updates. Expert lane memory-limit and OOM-recovery events were absent in this interval. Expert-only loss, scale, selection and related metrics are N/A.

| Update | PPO gradient norm mean / max | AMP scale | Combat entropy | RSS peaks per rank (bytes) |
| ---: | --- | ---: | ---: | --- |
| 2895 | 1.1491721607744694 / 2.0793161392211914 | 131072.0 | 0.2916675316053098 | 2495098880 / 2331893760 / 2348773376 / 2334937088 |
| 2896 | 1.032129216939211 / 1.6351981163024902 | 131072.0 | 0.2892509543658534 | 2495098880 / 2331893760 / 2348773376 / 2334937088 |
| 2897 | 1.2946454640477896 / 2.389741897583008 | 131072.0 | 0.28981068573968316 | 2495098880 / 2331893760 / 2348773376 / 2334937088 |
| 2898 | 1.8599127046763897 / 2.448563814163208 | 131072.0 | 0.291932376143482 | 2495098880 / 2331893760 / 2348773376 / 2334937088 |
| 2899 | 1.381170179694891 / 2.441953659057617 | 131072.0 | 0.29478055680719023 | 2495098880 / 2331893760 / 2348773376 / 2334937088 |

## Independent CPU audit

One CPU audit process completed in 3.1757354736328125 seconds with exit 0 and empty stderr. One strict model load verified that all 12,435,074 parameters were finite. The audit reconciled the four result shards, four ledger shards, combined ledger, paired references and all 60 training rows in the strict window. It reused the natural evaluation outputs. The separate 120-hour argmax/combat supplement will be reported after its final result.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-120.pt` | 49768265 | `eb7eba86962f495b3c9e8c09775cf1e262163c5d1e4d44f88ef20cdcdec9ee80` |
| `evaluation-2899-act2-ledger.json` | 1600239 | `c0466b095648add68190cb4069308f836bae759ae8b2ea2de31c57a59df9003d` |
| `evaluation120.audit.original.json` | 200803 | `2f572e84b73e33528ebcb248f27e91cfcce45bf09cab23b5d75f5f2cecc577a1` |
| `episode-fault5388-seed1306859274-after2848.json` | 4639 | `a66b6ad25ec604facf4192ca6a50fe8ae147ea0871946f4903362ca5a3c21bbb` |
| `episode-fault5388-seed1003920883-after2870.json` | 5190 | `1dfca82c6ff2f5369adb9dc0e22a41930ca7af937caf3d527b87e67a4fef4f2d` |
