# 交接文档：杀戮尖塔 1 · 静默猎手 · GPU 复刻与训练

更新：2026-10-17。主版本：`main@15db6bf`（加上这份交接文档）。

## 1. 项目是什么

- 把《杀戮尖塔 1》静默猎手（A20、三把钥匙、打心脏）**一比一**重写成能在 CPU 和 GPU 上批量跑的模拟器，然后用强化学习训练**一个神经网络打完整局**。
- 训练原则（主人定的）：
  - 打牌时不搜索，不 SL（存档读档）。
  - 网络只看玩家屏幕上能看到的信息，看不到随机数和抽牌顺序。
  - 只有真正打赢心脏才给 1 分，没有其他奖励。
- 正确性标准：以本机原版游戏为准，录像逐步对照，一个字节都不能差。
- 代码由并行的 GPT（Codex）写，Claude 负责派活、审查、核对、合并。

## 2. 东西都在哪

| 内容 | 位置 |
|---|---|
| 主仓库（唯一正式版本） | `/Users/destire/Documents/ChatGPT/sljt/sts-silent-gpu` |
| GPT 工作区（每个任务一个克隆） | `/Users/destire/Documents/ChatGPT/sljt/sts-silent-gpu-agents/<任务名>` |
| 派活说明 | `sts-silent-gpu-agents/tasks/<任务名>.md` |
| GPT 日志和最终报告 | `sts-silent-gpu-agents/logs/<任务名>.jsonl`、`<任务名>-last.md` |
| 训练方案调研 | `sts-silent-gpu-agents/research-training/`（`report.md`、`experiments.md` 定义 E0–E11） |
| **长期保存的数据** | `sts-silent-gpu-agents/artifacts/`（见下） |
| 原版游戏反编译源码（只读） | `/Users/destire/Documents/Codex/2026-09-10/new-chat-2/work/native-complete-source/source` |
| Python 环境 | `/Users/destire/Documents/Codex/2026-09-10/new-chat-2/outputs/spire-lab/.venv/bin/python` |
| Claude 的项目记忆 | `~/.claude/projects/-Users-destire/memory/sts-silent-gpu.md` |

`artifacts/` 里的东西（从 `/private/tmp` 拷出来的，因为 `/private/tmp` 重启会被清空）：

- `e2-roots/`：8,721 个后期练习起点，训练组和调参组各一份，另附 `native-audit.roots.jsonl.gz`。
- `e1/`：E1 的 3 个训练好的网络（`checkpoint-<初始化>.pt`）、起点、报告和学习曲线图。
- `deep-traces/`：12 局长局原版录像，按 10 月 17 日的新快照规则重录过，用来做深度核对。

`/private/tmp/e1-runs`、`/private/tmp/e2-roots` 里还有完整的过程数据，各约 2.3 GB，重启后可能就没了。

## 3. 现在做到哪了（都核对过）

**模拟器**
- 整局都能跑，和原版逐步对照：853 局录像，19,401 个决定，零差异；12 局长局，15,520 个决定，零差异。
- 测试 49 项全过。
- 本机速度（含翻译局面）：

  | 打法 | 1 核 | 8 核 |
  |---|---:|---:|
  | 随机打 | 85 万决定/秒 | 300 万 |
  | 打长局 | 60 万 | 191 万 |

**观察（网络能看到什么）**
- 只给可见信息，规则写在 `docs/observation.md`，有专门的"偷看检测"测试。
- 翻译代码已改成 CPU、GPU 通用的写法。

**训练程序**（`train/`）
- PPO 算法，只按输赢给分。
- 主人定了训练只用 GPU 版模拟器（10 月 17 日合并 `gpucollect`）：游戏、网络输入、采样、打牌记录都留在显卡上；每张卡一个进程，自己收集、自己算 DDP 那一份，零策略延迟。
- 随机选择用 Philox 计数器（按 根、尝试、第几步 算），换卡数、换批大小结果都一样；已对过 Random123 标准答案。
- CPU 版模拟器只作参照和 Mac 上的测试，走同一套收集代码。

**练习起点（E2）**
- 8,721 个，用"能看到未来"的搜索打出来，只用来造起点，不拿来当答案教网络。

  | 阶段 | 训练组 | 调参组 |
  |---|---:|---:|
  | 刚进第三幕 | 871 | 440 |
  | 刚进第四幕 | 385 | 179 |
  | 心脏战开场 | 254 | 130 |

**第一个小训练试验（E1）：过关**
- 起点离胜利 8 步、32 步：3 个网络在练过的起点上胜率从约 35% 练到 94%、99.5%、96%。
- 在没练过的起点上从约 43% 提到 60%、78%、75%。
- 不足：没练过的起点只有 6 个，波动大；离胜利 128 步的起点一个都没有。

**接真游戏的插件**（`watch/`）
- 模拟器和真游戏同步走，每一步都对照，一有差异就停。
- 有画面，可以看网络在真游戏里打。用法见 `README.md` 的 "Watch a checkpoint in the original game"。
- 用的是单独的一份游戏副本，不碰 Steam 的游戏和存档。

**显卡准备**
- 显卡版代码、CPU/GPU 逐字节对照测试（`tests/test_cuda_parity.cu`）、上机检查脚本（`tools/gpu_check.sh`）都写好了。
- 服务器需要装什么、怎么同步代码和数据，写在 `README.md` 的 "V100 day-one check"。
- **都还没在真显卡上跑过。**

## 4. 已知但不修的差异

原版游戏自己在这几处就不固定，结果会随动画快慢、空转帧数变化。我们的模拟器各选一种固定下来，都不影响输赢。

- 死亡那一刻手里保留牌的格挡数（例如"扫腿"原版自己时而 14、时而 10）。
- 弃牌堆里牌的伤害显示：统一按基础伤害记录。
- "把诅咒变成别的牌"时的预览会多消耗随机数：这条路线不进练习起点。
- 用烟雾弹逃跑后，"异教徒"怪物的仪式相关时机。

## 5. 没做完的

1. **上显卡集群**（Slurm 系统，用密码登录，主人不想用钥匙）。主人已经从另一台电脑连上，这台 Mac 连不到。查到的情况：一台机器 8 张 V100，每张 16 GB，驱动 580.105.08（最高支持 CUDA 13.0）。主人要**自己一步一步操作**，Claude 只负责指导，不替他去探测网络。
   - V100 不是原先准备的 A100：CUDA 工具包要用 12.8 或 12.9（13 不支持 V100），检查脚本 `tools/gpu_check.sh` 已改成按 V100（`sm_70`）编译，并会检查 PyTorch 带不带 V100 支持，测速的局数加到了 32,768。
2. 接下来：装好软件，同步代码和 `artifacts/e2-roots/native-audit.roots.jsonl.gz`，先跑 `tools/gpu_check.sh`（模拟器 CPU/GPU 逐字节对照 + 只测游戏的速度），再跑 `tools/gpu_train_check.sh`（训练链路 CPU/GPU 对照、单卡网格测速、1/2/8 卡弱扩展；编译后的测量限 30 分钟）。用法见 README "V100 day-one check"。
3. 训练实验目前**按主人要求暂停**。E1 第二轮做了一半，在 `sts-silent-gpu-agents/e1` 的 `e1` 分支上，未合并。接下来按 `experiments.md` 是 E3（从后往前推的练法对比直接从开局练）。
4. 完整训练（从开局一路打赢心脏）还没开始。留作最终考试的种子一次都没用过。

## 6. 怎么核对（每次合并前都要做）

```sh
P=/Users/destire/Documents/Codex/2026-09-10/new-chat-2/outputs/spire-lab/.venv/bin/python
B=/tmp/某个新的构建目录
cmake -S . -B $B -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DPython_EXECUTABLE=$P
cmake --build $B --parallel 6
ctest --test-dir $B --parallel 1                     # 应为 49/49
PYTHONPATH=$B $P tests/replay.py tests/traces        # 应全部通过
PYTHONPATH=$B $P tests/replay.py ../sts-silent-gpu-agents/artifacts/deep-traces   # 12/12
for t in packing distributed pipeline policy; do PYTHONPATH=$B:. $P -m pytest -q tests/test_train_$t.py; done
PYTHONPATH=$B:. $P tools/benchmark_training.py --mode both --threads 1 8 --size 1024 --seconds 5
```

GPT 的报告不算数，必须自己重新编译、重跑才算核对过。

## 7. 怎么派 GPT

```sh
CODEX=/Applications/ChatGPT.app/Contents/Resources/codex-cli/CodexCLI.app/Contents/MacOS/codex
A=/Users/destire/Documents/ChatGPT/sljt/sts-silent-gpu-agents
# 新任务：先 git clone 主仓库到 $A/<名字> 并建同名分支
$CODEX exec -m gpt-6-astra -c service_tier='"default"' -c model_reasoning_effort='"xhigh"' \
  -s danger-full-access -C $A/<名字> -o $A/logs/<名字>-last.md --json - < $A/tasks/<名字>.md > $A/logs/<名字>.jsonl 2>&1
# 接着上一次的对话：先 cd 进克隆目录（resume 不接受 -C）
cd $A/<名字> && $CODEX exec resume <thread_id> -m gpt-6-astra -c service_tier='"default"' \
  -c model_reasoning_effort='"xhigh"' -c sandbox_mode='"danger-full-access"' -o ... --json - < 说明 > ...
```

- 用 ChatGPT.app 自带的 codex，不用 brew 装的那个。**不用 fast 档**。
- 一般任务用 `gpt-6.1-sol`，要紧任务用 `gpt-6-astra`。
- 派活说明里要写"不要给进程挂调试器"。OpenAI 的安全过滤会把挂调试器当成攻击行为，直接掐断任务。
- 本机最多同时 3 个重活。
- 不 push，不开 PR。合并由 Claude 在主仓库里做。

## 8. 必须遵守的规矩

- 跟主人说话用简单的中文，称呼"主人"，先说结论，术语要解释，分清"我核对过的"和"GPT 说的"。
- 不动代理、DNS、VPN、镜像，不开关 Docker；下载慢就停下来报告。
- 不动 Steam 游戏和存档，不动 `Documents/Codex` 和 `Documents/ChatGPT/sljt` 下除本项目仓库和克隆以外的东西。
- 不改、不停、不释放阿里云资源，不租机器。不输出任何私钥内容，不把主人的密码写进任何文件。
- 仓库规矩见 `AGENTS.md`：不写多余的防御代码，不留死代码，只提交源码和小测试。
