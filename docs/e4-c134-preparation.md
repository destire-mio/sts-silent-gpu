# Act3-start pool preparation for Claude134

## Claude160: prepare 768×12; do not switch

下次大模型为宽768、12层、12头、前馈3072；512专用分支已移除。
`tools/e4_large_prepare.py --width 768` 只在指定的隔离输出目录初始化学生。
rank 数来自一致检查点的 `state.world`（1–8），只处理对应 rank 文件，不复制目录中残留的旧 rank。
教师按完成的 argmax dev 平均层数选择最好的384×6，严格加载；蒸馏目标40轮。
价值系数沿用512方案：保留旧模型已校准目标，completed_updates=20，不重新跑旧验收。

当前只做代码、合成检查点的CPU测试和显存组件估算，不读取当前大rank、不生成实际训练学生、
不绑定大模型源、不申请GPU。CPU基准的显存估算给出输入形状和FP32模型/梯度/Adam、
激活检查点及单层工作区；16GiB是保守容量参考。它不等于实测GPU峰值，
也不覆盖原生车道、rollout、缓存、教师及CUDA工作区，长输入须另核算。

以后切换步骤（此处不执行）：

1. 先从实际自然后继日志确认 #159 生效 update。此后的常规考试连续三次不涨时，
   由Claude决定切换时机；不把 #159 之前的考试算入，不自动触发。
2. 等当前作业自然保存结束，保留一致的model及state.world个rank原始备份和费用记录。
   在隔离目录运行 `python -m tools.e4_large_prepare --source BACKUP --output STUDENT --teacher BEST384 --width 768 --small-seconds LAST_SMALL_ROUND_SECONDS`。
3. 核对准备回执、教师dev成绩/strict load、rank身份、保留的池/RNG/训练时钟和40轮蒸馏目标。
   新学生与Adam从零开始，旧小模型及未完成learner留在备份；价值校准目标沿用旧模型。
4. 获得当时的切换授权后才通过原控制器、原锁绑定自然续跑；保留原资源/费用/评估规则。
   显存估算不是GPU验收；切换时按实际输入和原有分配核对显存与microbatch，禁止额外GPU试跑。

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
