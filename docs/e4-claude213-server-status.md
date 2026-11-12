# #213 服务器作业与 46 小时权重

**当前作业。** 北京时间 2026-11-13 00:27:06，Slurm 显示 5600 在跑，实际 runtime 为 `d575609a06ad88418c79bfaf8e99928aec966e02`。这是 #186 旧对照配方，熵系数 0.01，未启用 #203 的 strategic share 覆盖、late pools 和 key-last-chance。最新完整训练为 update 1471，累计 172878.41036711982 训练秒。两张卡正在推进原定的 48 小时专家补测，5601 等待接续。

**回退。** 11 月 12 日 08:17:39，5559 用 #203 的 `a1cc559c15248b34b89c078b8dacdf9a96e4ebbd` 从 update 1366 启动。09:51:23，5559 在 46 小时 raw 测评的逐局汇总中退出。5560 和 5561 继续用同一 runtime，分别于 09:58:58 和 10:06:31 因同一错误退出。Slurm 对三单均记录 `FAILED 1:0`。三单的 rank 0 终止日志一致。

```text
[rank0]:     for act, counts in episode['counts_by_act'].items():
[rank0]: KeyError: 'counts_by_act'
```

10:10:42 按故障回退流程显式将控制器绑定到 #186，事件为 `claude203_original_recipe_rollback_bound`。11:32:06，5562 用旧配方从同一自然检查点接续。

**46 小时权重。** 5559 用 #203 训练了 update 1367–1410，共 44 次完整更新，保存了 update 1410、serial 146 的 raw 和 EMA 权重。5562 继承 Adam step 22568，用旧 runtime 在 165629.23454487184 训练秒对这两份权重各补测 1024 局，实跑 key-last-chance 关闭。`job-5562.log` 第 67、88 行记录 raw、EMA 完成，第 105 行才是回退后首次优化更新 1411。[46 小时审计](https://github.com/destire-mio/sts-silent-gpu/blob/fd7a011c4424ec4a0a8113ba0849d6b9efb95914/docs/e4-claude186-control-46h.md)
