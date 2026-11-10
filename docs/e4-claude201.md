# E4 #201 实验命令

从 `codex/e4` 的独立运行目录执行。将 `DEV112` 指向服务器 dev-112 的原始权重副本。GPU 运行环境需提供仓库的原生 `spire_gpu` 模块。种子范围采用左闭右开。

## 价值残差

```bash
python -m tools.e4_value_residual "$DEV112" \
  --seeds 1000000000:1000002048 --output artifacts/e4-201/value-residual
```

`value-residual.json` 包含五个里程碑的逐局值、实际剩余回报和分组 SE，单位为原奖励分。每组按该里程碑内有完整结局的训练局计数。卡牌升级沿用同一个卡牌 ID。能力牌分为 0、1、2、3+。

## 选路金标准和训练标签

```bash
python -m tools.e4_map_labels "$DEV112" --gold 512 \
  --seeds 1000000000:1000002048 --output artifacts/e4-201/map-gold

python -m tools.e4_map_labels "$DEV112" --labels 1024 \
  --seeds 1000100000:1000102048 --output artifacts/e4-201/map-labels
```

将输出放在持久化盘上。用相同命令恢复，已完成的来源分片和估值分片会被复用。金标准每选项 128 条，64 条挑选、64 条评估，SE 按来源局聚类。金标准和标签的收益为归一化奖励，乘 156 得原奖励分。标签先为每个选项测 4 条，再给保留的一半和配对 argmax 参考各补 8 条。

## 独立 PPO 蒸馏

```bash
torchrun --standalone --nproc_per_node=1 -m tools.e4_stage12 \
  --init "$DEV112" --output artifacts/e4-201/map-distill \
  --map-distill artifacts/e4-201/map-labels --map-distill-weight 1

torchrun --standalone --nproc_per_node=1 -m tools.e4_stage12 \
  --init "$DEV112" --output artifacts/e4-201/map-control \
  --map-distill artifacts/e4-201/map-labels --map-distill-weight 0
```

训练分叉使用独立的输出目录。恢复时沿用同一目录、标签内容和权重。日志 `training.jsonl` 记录蒸馏 KL、头输出绝对值均值、选路 PPO KL 及样本数，并按普通起点和中段起点分别给出第四幕、绿钥匙的计数和分母。检查点同时保存选路头和它的 Adam 状态。

## 选牌三档视野

```bash
python -m tools.e4_macro_audit "$DEV112" --horizons act,next_act,full \
  --horizon-pilot --horizon-k 8 --horizon-seeds 1000000000:1000001024 \
  --output artifacts/e4-201/card-pilot

python -m tools.e4_macro_audit "$DEV112" --horizons act,next_act,full \
  --horizon-pilot-report artifacts/e4-201/card-pilot/card-horizons.json \
  --horizon-k 8 --horizon-seeds 1000000000:1000001024 \
  --output artifacts/e4-201/card-audit
```

先在目标设备跑 30 分钟测速，再用测速报告给出的根数运行。审计有六小时墙钟上限，汇总报告记录实际根数和 K，原始分片保留路径、种子和各选项的逐条回报。改进用前半选、后半评。指定卡牌与 argmax 的差使用所有有效配对，每个来源局贡献一个根。三个视野共享同一条续玩轨迹。终局记实际回报，本幕或下一幕非终局边界加 V。

正式规模的 G4 测速和统计报告待独立运行环境。当前验证为原生 CPU 冒烟。
