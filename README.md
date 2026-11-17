# sts-silent-gpu

把《杀戮尖塔 1》静默猎手（A20、三把钥匙、打心脏）**逐字节一比一**重写成 C++ 模拟器：同一份代码既能在 CPU 上跑，也能让每个 GPU 线程各跑一整局。然后用强化学习从零训练一个网络，让它打完整局。

*An exact C++/CUDA re-implementation of Slay the Spire 1 (Silent, Ascension 20, three keys, Heart) where every GPU thread owns one complete game, plus a from-scratch PPO agent trained on it. Target build: original Java `[V2.3.4] (12-18-2022)`.*

## 一眼看懂

| 项目 | 结果 |
|---|---|
| 模拟器正确性 | 同一个种子、同样的操作，每一步都和原版游戏的完整快照逐字节一致（录像回放对照，无容差） |
| CPU / GPU 一致性 | 12 局固定深层路线 15,038 步、8,448 个随机批次槽位，整个 `Game` 字节、观测、奖励、候选动作全部一致 |
| 模拟速度 | 单张 V100 随机动作 **约 68 万决策/秒**；八卡完整训练（采样+推理+PPO）**约 5 万新决策/秒** |
| 最好模型（不搜索、不 SL） | 1024 个新种子平均走到 **第 34.3 层**；**Heart 胜 3/1024（0.29%）** |
| 原版游戏对接 | `watch/` 能把模型接进原版游戏逐步对照运行；已在原版里接管一场心脏战并打赢（训练起点，仅 7 步，只证明对接通了，不代表实力） |

### 最好模型的考试成绩

1024 个固定新种子（2000000000–2000001023），argmax 出牌，无搜索、无 SL，钥匙只在最后机会强制。

| 关卡 | 通过 |
|---|---:|
| 第一幕 Boss | 88.5% |
| 第二幕 Boss | 35.2% |
| 第三幕第一个 Boss | 9.0% |
| 集齐三把钥匙 | 232 / 1024 |
| 进入第四幕 | 27 / 1024 |
| 击败盾与矛 | 21 / 1024 |
| **击败 Heart** | **3 / 1024** |

模型：观测版本 14，dev-78 回滚线上纯 PPO 续训后取 EMA(.998) 权重。权重不放在本仓库。

## 训练规则（一直遵守）

- **只看屏幕上能看到的信息**：看不到随机数、抽牌顺序、怪物还没显示的行动。只有训练用的打分器（critic）允许看隐藏信息，出牌的网络和考试永远看不到。
- **不 SL**（不存档读档），训练和考试都一样。允许"确定化前瞻"（随机猜一个未来去推演）。
- **奖励只给没有代价的进度**：楼层、Boss、第四幕、盾与矛、Heart。钥匙、血量、药水、遗物一律不手写分值（练习局内允许算掉血）。
- **不用人类数据、不用 LLM 教练、不手写"好卡组"**。

## 哪些东西有效

| 改动 | 效果（1024 局配对，平均层数） |
|---|---|
| 加大价值头系数（c = 181） | **+1.6 层**，最大的一次提升 |
| EMA(.998) 权重考试 | **+0.85 层** |
| 学习率封顶 4.44e-5 | 阻止了更高学习率下的退步 |

## 试过但没用的（2026-11-16 ~ 17，都和对照组配对比较）

| 方法 | 结果 | 结论 |
|---|---|---|
| 只算输赢的 Boss 专练（25% 样本），6h | +0.14 ± 0.82 层 | 持平 |
| 带掉血的 Boss 专练 + 独立练习价值头，4h | **−0.82 ± 0.82 层** | 更差，提前停 |
| 特权 critic（看真实抽牌顺序和怪物预定行动），6h | **−0.97 ± 0.86 层** | 更差；critic 解释方差 .90 vs .77，噪声确实降了，但策略没变强，且慢 40% |
| 多步搜索 plan64 vs 一步搜索 8×4（Boss 战，32 根 × 4） | 第二幕 −1.6 ± 3.9pp，第三幕 −0.8 ± 2.6pp | 不比一步搜索强；一步搜索比直接出牌只多 +3.9pp |
| 选牌打分器（9,400 根训练） | **−6.86 ± 0.44 层** | 同根排序准确率仅 .59–.67，比模型自己选还差 |
| 自然决策点分支实测（选牌、路线、营火；去偏后） | 最好分支 − 策略 ≈ −2.7 ~ +1.5pp | 模型自己的选择已接近局部最优；打精英反而更差 |
| 一步搜索的改选是否稳定（2,048 个战斗决策） | 4 次独立搜索两两一致率 42%，稳定改选仅 2.7% | 没有可蒸馏的稳定目标 |

服务器报告：[选牌打分器](https://github.com/destire-mio/sts-silent-gpu/blob/codex/e4/docs/e4-deck-eval-server.md)、[分支实测](https://github.com/destire-mio/sts-silent-gpu/blob/codex/e4/docs/e4-branch-audit-server.md)、[搜索稳定性门槛](https://github.com/destire-mio/sts-silent-gpu/blob/codex/e4/docs/e4-search-target-gate.md)。

**目前的判断**：模型在每个局部决策上都已接近它自己能看到的最好；输赢主要在进 Boss 前就由整体实力决定（第三幕有一半的根，任何打法 4 次全输）。对照组从起点练 6 小时也没有超过起点，现有训练方法在这个算力下已到平台期。

## 仓库导航

| 位置 | 内容 |
|---|---|
| `core/` | 固定容量的 C++17 局面与战斗状态、RNG、队列、各卡牌/遗物/怪物规则 |
| `obs/` | 设备端可见信息编码器 |
| `host/` | CPU 批量驱动、Python 绑定、快照序列化、测速 |
| `cuda/` | CUDA 批量内核 |
| `train/` | 策略网络、GPU 采样器、DDP PPO、考试 |
| `tests/` | 原生测试、设备代码规范检查、严格录像回放 |
| `oracle/`、`watch/` | 原版游戏录像器；把模型接进原版游戏逐步对照运行 |
| `inventory/` | 内容清单与验证记录 |
| `docs/` | 设计与报告，见下 |

- 编译、测试、训练命令和全部工程细节：[docs/engineering.md](docs/engineering.md)
- 设计：[docs/design.md](docs/design.md)，观测：[docs/observation.md](docs/observation.md)，移植指南：[docs/porting.md](docs/porting.md)
- 一致性与测速：[docs/cpu-gpu-parity-2026-10-18.md](docs/cpu-gpu-parity-2026-10-18.md)
- 训练计划：[docs/e4-plan.md](docs/e4-plan.md)
- 开发规则：[AGENTS.md](AGENTS.md)

最新训练代码在 [`codex/e4`](https://github.com/destire-mio/sts-silent-gpu/tree/codex/e4) 分支；`main` 保存引擎、文档和沟通记录。

## 快速编译

需要 Clang 21、CUDA 12.8、CMake ≥ 3.31、Python + pybind11：

```sh
# CMAKE_CUDA_ARCHITECTURES 按你的 GPU 改（V100 = 70）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CUDA_COMPILER=clang++ \
  -DCUDAToolkit_ROOT=/usr/local/cuda-12.8 -DCMAKE_CUDA_ARCHITECTURES=70 \
  -DPython_EXECUTABLE=$(command -v python) -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

完整说明（V100 工具链、设备链接检查、训练与考试命令）见 [docs/engineering.md](docs/engineering.md)。

## 许可

见 [LICENSE](LICENSE)。本仓库不包含任何原版游戏文件。
