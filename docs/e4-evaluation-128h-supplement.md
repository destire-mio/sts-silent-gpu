# E4 128-hour supplemental combat comparison

Jobs 5431 and 5432 evaluated the same frozen 768×12 `dev-128.pt` after the deferred exam resumed. Both jobs used two V100s. The evaluation made zero parameter updates.

| Mode | Completed seeds | Mean floor across completed seeds | Recorded time |
| --- | ---: | ---: | ---: |
| Argmax | 128/128 | 26.6171875 | 103.91687010903843 s |
| Combat lookahead | 30/128 | 30.2 | 3603.5319683640264 s |

Combat lookahead ended at the one-hour cap, with 98 seeds excluded from the paired comparison. Timer accounting was complete across the natural 5431 and 5432 windows, which contributed 2184.2402495159768 and 1419.2917188480496 seconds. The recorded sum includes mode initialization and the final saved timing interval.

On the 30 common seeds, mean floor rose from 26.266666666666666 to 30.2. The paired increase was **3.933333333333333 floors** with SE **1.8469815906414937**. Lookahead finished deeper in 15 cases, tied in 9, and finished shallower in 6. Neither mode reached Heart on these paired cases. The estimate describes the seeds that finished within the cap.

Lookahead recorded 29,295 mainline decisions, 16,255 preview decisions, 2,165 overrides, and 11,252,304 preview steps. The per-preview SE=0 fraction was unavailable. The first-preview-fault JSON was absent at the natural final.

An independent CPU audit reconciled all 46 JSON files, both timing windows, the frozen checkpoint SHA-256 `288a33f941d8272e10602e0468f4b27982680fc1155a51796c97f9ddfbd02be4`, and the paired seed metrics.
