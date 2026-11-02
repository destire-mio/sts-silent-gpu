# Act3-start pool preparation for Claude134

Claude134 adds a fourth256-entry pool at the first Act3 decision, for the next natural resume. Training starts use45/10/20/15/10 for fresh/Act1Boss/Act2-start/Act2Boss/Act3-start. Until an optional later pool reaches32 entries, its share routes to Act2-start; the existing first-two-pool warmup remains. Stored native pool slots increase from768 to1024 per rank; probe storage stays separate.

Three-pool checkpoints migrate by preserving existing collector state, RewardLedger, pools, randomization history and RNG, while adding an empty fourth pool. Resumed lanes already in Act3 are marked captured and do not contribute a late Act3-entry snapshot. The existing two-pool migration remains supported.

Five affected CPU checks passed using the existing native game backend: mix/warmup, old pool capacity and probe separation, two-pool migration/Act2Boss capture, three-pool migration with native bytes/RewardLedger/collector RNG preserved and first Act3 capture, and actual fourth-slot native clone/restart. Four checks passed together; the added fourth-slot check passed separately, reusing the earlier results. No benchmark or GPU training/evaluation run was launched.

Only the midstart collector, worker storage-slot count and affected tests changed. Model/learner, rewards, value calibration/diagnostics, checkpoint transaction code and controller rules are byte-identical to the previous runtime. The isolated candidate source isde0c50507c55a55e527a6796ccec2f918b9d6b12. Binding this candidate for the next natural resume does not establish that it is already active; actual runtime, matching checkpoint restore and new finite PPO must be checked after that worker starts.
