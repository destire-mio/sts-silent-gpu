# Act3-start pool preparation for Claude134

## Claude159: whole-run randomization on restart

中途开局已用整局随机化（#159）：克隆池存档后，首次 choose 即调用
`determinize_slots(slots, seeds, True)`，包括尚未进入战斗的车道。
随机化记录为 `(position, seed, run)`；旧二元组仍按 `run=False` 重放。
续跑恢复、池状态恢复和 fault 重放共用这条重放路径；同一位置的多条记录按原顺序应用。
池内原始状态、奖励、训练参数和优化器逻辑保持原样，不清池或回滚。

本变更只绑定下次自然续跑。当前 5341 的 478896/c173 不包含 #159；
首个实际生效 update 待自然后继的小日志 `midstart_randomization=whole_run_159`
与真实 runtime source 确认后记录，不提前声称当前 worker 已切换。

Claude134 adds a fourth256-entry pool at the first Act3 decision, for the next natural resume. Training starts use45/10/20/15/10 for fresh/Act1Boss/Act2-start/Act2Boss/Act3-start. Until an optional later pool reaches32 entries, its share routes to Act2-start; the existing first-two-pool warmup remains. Stored native pool slots increase from768 to1024 per rank; probe storage stays separate.

Three-pool checkpoints migrate by preserving existing collector state, RewardLedger, pools, randomization history and RNG, while adding an empty fourth pool. Resumed lanes already in Act3 are marked captured and do not contribute a late Act3-entry snapshot. The existing two-pool migration remains supported.

Five affected CPU checks passed using the existing native game backend: mix/warmup, old pool capacity and probe separation, two-pool migration/Act2Boss capture, three-pool migration with native bytes/RewardLedger/collector RNG preserved and first Act3 capture, and actual fourth-slot native clone/restart. Four checks passed together; the added fourth-slot check passed separately, reusing the earlier results. No benchmark or GPU training/evaluation run was launched.

Only the midstart collector, worker storage-slot count and affected tests changed. Model/learner, rewards, value calibration/diagnostics, checkpoint transaction code and controller rules are byte-identical to the previous runtime. The isolated candidate source isde0c50507c55a55e527a6796ccec2f918b9d6b12. Binding this candidate for the next natural resume does not establish that it is already active; actual runtime, matching checkpoint restore and new finite PPO must be checked after that worker starts.
