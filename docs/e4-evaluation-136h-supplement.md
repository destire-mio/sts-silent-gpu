# E4 136-hour supplemental combat comparison

Job 5434 used two V100s to evaluate the frozen 768×12 `dev-136.pt` at update 91. The supplement made zero parameter updates.

| Mode | Completed seeds | Mean floor across completed seeds | Recorded time |
| --- | ---: | ---: | ---: |
| Argmax | 128/128 | 31.359375 | 109.33970918599516 s |
| Combat lookahead | 44/128 | 35.77272727272727 | 3602.2628228800604 s |

Combat lookahead reached the one-hour cap with complete timer accounting for the natural window. The recorded time includes initialization and the final saved timing interval. The cap excluded 84 seeds from the paired comparison.

On the 44 common seeds, mean floor rose from 30.772727272727273 to 35.77272727272727. The paired increase was **5 floors** with SE **1.6657855317033268**. Lookahead finished deeper in 21 cases, tied in 19, and finished shallower in 4. Neither mode reached Heart in the paired cases. This estimate covers the seeds that finished within the cap.

Lookahead recorded 36,348 mainline decisions, 17,993 preview decisions, 2,347 overrides, and 12,077,368 preview steps. The per-preview SE=0 fraction was unavailable. The first-preview-fault JSON was absent at the natural final.

An independent CPU audit verified all 37 JSON files, the frozen checkpoint SHA-256 `6e67e04faf55aa0409a2e0de2703543db167b56a658fccb236b62888c05582d7`, and the seed-level and timing summaries.
