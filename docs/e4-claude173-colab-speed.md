# E4 #173 Colab 单卡测速

脚本和测试提交 `1d16099d8ee1b02599644bfb4ea5283254d8b613`。从仓库根目录运行，例如：

```bash
PYTHONPATH=build:. python -m tools.colab_speed --width 768 --lanes 2048 --microbatch 256 --recompute on --dtype fp16 --rounds 3
```

省略 `--checkpoint` 时使用固定随机种子初始化。指定权重文件时按对应宽度严格加载。每轮采样 64 步，PPO 使用 32768 minibatch、2 个 epoch，输出采样和 learner 时间、决策吞吐、峰值显存与故障数。脚本只在内存中更新模型。

集群 CPU 冒烟测试在隔离目录使用 c196 的训练库和现成 CPU 引擎。384 宽度、2 个 lane、随机权重跑完一轮，128 次有效决策，0 fault。pytest 1 passed / 21.11 秒，退出码 0，stderr 为空。pytest 记录了一条既有的 NumPy 只读视图警告。A100 和 H100 的 GPU 测速待 Colab 实跑。
