# V100 GPU 游戏模拟测速与修复报告

2026 年 10 月 18 日，在单张 Tesla V100-SXM2-16GB 上完成 GPU 游戏模拟器编译和随机合法动作测速。实测最高 **680,549 次决策/秒**，对应 32,768 个并行环境。六个规模累计推进 **17,734,176 次决策**，计时阶段故障和截断均为 0。

本次测量覆盖游戏推进、状态编码和批量调度，可用于评估模拟器吞吐。神经网络推理、轨迹存储和 PPO 更新不在计时范围内，因此该数字不能直接当作训练速度。

## 测试版本与环境

| 项目 | 实际配置 |
|---|---|
| 测试代码 | [`52d625785f0c62f39c7d91694862700dd905e6ea`](https://github.com/destire-mio/sts-silent-gpu/commit/52d625785f0c62f39c7d91694862700dd905e6ea)，构建和测速时工作区干净 |
| 基础代码 | `split` 分支的 `fbceb8c7388c795ff699b511a42db77be900a7a7`，叠加下文修复 |
| GPU | 1 × Tesla V100-SXM2-16GB，目标架构 `sm_70` |
| Slurm 分区 | `N56_128G_V100` |
| CUDA 编译器 | Clang 21.1.7 |
| CUDA Toolkit | 12.8.61 |
| CMake | 3.31.8 |
| Python / pybind11 | 3.12.12 / 3.1.0 |
| PyTorch | `2.14.0+cu126`，`torch.version.cuda = 12.6` |
| 构建参数 | Release、`-O3`、device inline threshold 5、RDC、无 device LTO |
| 构建目标 | 仅 `spire_gpu`，`BUILD_TESTING=OFF`，2 个并行编译任务 |
| 构建作业 | 3496，分配 32 CPU 核、112 GB 主机内存 |
| 测速作业 | 3497，分配 2 CPU 核、32 GB 主机内存、1 张 V100 |

Toolkit 12.8.61 是编译扩展所用的工具链版本；PyTorch 的 12.6 是其 CUDA 构建版本。两者含义不同，本次组合已完成实际 GPU 执行。

构建从北京时间 17:15:03 到 17:20:47，`/usr/bin/time -v` 记录墙钟时间 **5:43.99**，退出码 0。分配 32 核并不等于同时运行 32 个编译进程：当前构建入口固定 `--parallel 2`。此耗时只描述本轮构建，不作为其他机器或代码版本的保证。

生成模块为 `spire_gpu.cpython-312-x86_64-linux-gnu.so`，大小 18,910,048 字节，SHA256：

```text
016133419d251ca937569fea163eaffc31e17d39d80994f43278bc48731fcfe5
```

## 测速方法

调用测试版本的 [`tools/benchmark_cuda.py`](../tools/benchmark_cuda.py) 中的 `measure(size, 5, None)`，按 32、128、512、2,048、8,192、32,768 顺序测量。

- 每个规模先预热 10 批；正式计时至少运行 400 批且至少持续 5 秒，两个条件均满足才结束。
- `TrainingBatch` 的单局决策上限为 3,000，初始种子从 `1_000_000_000` 连续取值，运行中补充种子池。
- 每个规模设置 `torch.manual_seed(1729)`，在 GPU 上随机选择当前合法动作。不同规模的轨迹和迭代次数不同。
- 决策数累加 `advanced` 标志，吞吐为实际推进决策数除以墙钟秒数，终局和重置操作的耗时包含在内。
- 计时包括 GPU 动作选择、调度、重置、游戏推进、状态编码、同步及计数开销。计时阶段 `game_downloads=0`；此前定位错误时的状态下载不属于测速。
- 故障标志在计时循环中累计；出现故障或截断时测速会失败。Python 在 CPU 上发起批量调用，游戏状态推进由 CUDA 内核执行。

这是每个规模各一次的测量，没有重复试验均值或置信区间。固定随机种子不能使受最短时间控制的运行拥有完全相同的迭代次数。

## 测速结果

| 并行环境数 | 计时秒数 | 批次数 | 实际决策数 | 决策/秒 | 终局次数 | 故障 | 截断 |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 32 | 5.0014 | 950 | 29,849 | 5,968 | 552 | 0 | 0 |
| 128 | 5.0019 | 785 | 98,707 | 19,734 | 1,774 | 0 | 0 |
| 512 | 5.0062 | 669 | 336,299 | 67,177 | 6,238 | 0 | 0 |
| 2,048 | 5.0020 | 586 | 1,178,509 | 235,607 | 21,664 | 0 | 0 |
| 8,192 | 5.9628 | 400 | 3,217,953 | 539,670 | 58,998 | 0 | 0 |
| 32,768 | 18.9154 | 400 | 12,872,859 | **680,549** | 234,923 | 0 | 0 |

累计 17,734,176 次决策、324,149 次终局。终局包含死亡，不能解释为通关次数或胜率。随机策略可能较早死亡，该工作负载也不能替代固定深层路线测速。

环境数从 8,192 增至 32,768，增加 4 倍，实测吞吐提升约 **26.1%**。32,768 是本次测过的最高吞吐配置；正式训练加入模型和轨迹存储后，需要重新确定合适的批量大小。本轮没有测量显存峰值、CPU 对照或多卡扩展效率。

完整浮点数值、运行元数据和各阶段时间戳见同目录的[原始 JSONL](gpu-benchmark-2026-10-18.jsonl)。

## GPU 运行错误与修复证据

### 不可变数据按值返回时出现非法写入

早期版本已能编译，但运行到小型探针的第 7 步时发生 `CUDA: an illegal memory access was encountered`。Compute Sanitizer 定位到 `AcidSlime_S::data()` 中的 1 字节 local-memory 非法写入，调用链经过怪物回合、游戏推进和 `spire_training_step`。

[`b5ad346`](https://github.com/destire-mio/sts-silent-gpu/commit/b5ad346685740fabfae12c1bb0d4968ab3ffb5e9) 将生成的不可变 `data()` 访问器从按值返回改为 `const DataType&`。底层常量表保持原有数据，需要修改数据的调用方仍可显式复制。修改后原小型内存检查通过。现有证据确定了故障位置和有效修复，尚不足以把第一个错误归结为某个已独立复现的编译器缺陷。

### union 返回值遗漏字节导致毒层数损坏

修复上述非法访问后，随机运行仍会触发 `NumericRange`，错误码为 `0xe0000006`。诊断捕获到 `Poison.amount = 1569432654`；进一步追踪发现，数值在传入施加能力函数之前就已损坏。

编译生成的 PTX 给出了直接证据：`BouncingFlask::construct` 返回的 `Effect` 中，偏移 16 处的 `amount` 是 4 字节整数，旧版本只返回了其中 1 字节，调用方随后按 4 字节读取。

```text
修复前 b5ad346：st.param.b8  [func_retval0+16], %r1;
修复后 52d6257：st.param.b32 [func_retval0+16], %r1;
```

[`52d6257`](https://github.com/destire-mio/sts-silent-gpu/commit/52d625785f0c62f39c7d91694862700dd905e6ea) 在生成的状态 union 和 `EffectPayload` 中，将覆盖完整载荷、保持原有对齐的连续整数数组放在首个数据成员位置，使聚合返回表示覆盖每个载荷字节。当前状态使用 1 或 4 字节对齐；字段访问方式保持一致，修复前后 `sizeof(Game)` 均为 37,872 字节。

最终版本保留原有故障检查。定位期间添加的临时错误码、状态探针和诊断改动已移除，测速没有通过裁剪异常数值或跳过故障环境来获取结果。

### 本次推送包含的构建修复

| 提交 | 作用 |
|---|---|
| `37863f4` | 增加仅构建 `spire_gpu` 的入口，完整检查脚本提前检查 host linker |
| `dc500e9` | 使用 `--no-offload-new-driver`，让 Clang RDC 对象格式与当前 CMake 的 nvlink 流程匹配 |
| `3f1071d` | 大型初始状态使用带 `const` 载荷的 device global storage，避免挤入 64 KiB constant bank |
| `510c9bd` | 补齐无需 Toolkit 的设备检查所需 `__constant__` 声明 |
| `8358b0c` | 将 host kernel launch 排除出 device 编译，消除内核符号类型冲突 |
| `a840f9a` | 将 CPU 深层路线准备延迟到完整测速入口，允许单独导入 GPU 随机测速函数 |
| `b5ad346` | 不可变单位数据通过常量引用访问 |
| `52d6257` | 保证 union 载荷在 CUDA 聚合复制和返回中完整保留 |

## 已完成的运行检查与适用范围

| 检查 | 覆盖范围 | 结果 |
|---|---|---|
| Compute Sanitizer memcheck | 32 个环境，连续 10 步选择候选动作 0 | `ERROR SUMMARY: 0 errors`，环境故障均为 0 |
| 随机运行探针 | 128 个环境，1,000 批随机动作 | 无故障完成 |
| 正式吞吐测量 | 表中六种规模，共 17,734,176 次实际决策 | 故障 0，截断 0，测速作业退出码 0 |

Memcheck 只覆盖小型探针，没有覆盖全部 1,773 万次计时决策。本轮结果证明该版本在上述工作负载下可以执行 GPU 模拟和编码；原版游戏 oracle 回放、完整 CPU/GPU 逐字节一致性、深层固定路线、训练更新和多卡运行仍需各自的验证。接入训练后的速度应按采样加更新的完整耗时统计。

## 复现本次随机测速

先在服务器已有的仓库中准备独立源码目录，固定到实际测试提交：

```bash
git fetch origin codex/fix-split-build-entry
git worktree add --detach "$HOME/sts-silent-gpu-bench-52d6257" \
  52d625785f0c62f39c7d91694862700dd905e6ea
```

将下面脚本保存到仓库外的 `~/spire-gpu-benchmark.sbatch`。它使用已安装的 `~/spire-env`、Clang 21.1.7 和 `/usr/local/cuda-12.8`，在同一 Slurm 作业内编译并测量；构建和预热均不计入吞吐。此复现脚本运行随机测速，不包含上表的两个额外探针。

```bash
#!/usr/bin/env bash
#SBATCH --job-name=spire-gpu-bench
#SBATCH --partition=N56_128G_V100
#SBATCH --gres=gpu:V100:1
#SBATCH --cpus-per-task=2
#SBATCH --mem=32G
#SBATCH --time=00:30:00
#SBATCH --output=spire-gpu-bench-%j.log
set -euo pipefail

repo="$HOME/sts-silent-gpu-bench-52d6257"
build="$HOME/spire-bench-build-$SLURM_JOB_ID"
export SPIRE_PYTHON="$HOME/spire-env/bin/python"
export PATH="$HOME/.local/bin:/usr/local/cuda-12.8/bin:$PATH"
export LD_LIBRARY_PATH="/usr/local/cuda-12.8/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 PYTHONUNBUFFERED=1
export TMPDIR="${SLURM_TMPDIR:-/tmp}/spire-bench-$SLURM_JOB_ID"
mkdir -p "$TMPDIR"
cd "$repo"
test "$(git rev-parse HEAD)" = 52d625785f0c62f39c7d91694862700dd905e6ea
git diff --quiet
git diff --cached --quiet

/usr/bin/time -v bash tools/gpu_build.sh "$build"
export PYTHONPATH="$build:$repo/tools:$repo"
sha256sum "$build"/spire_gpu*.so

"$SPIRE_PYTHON" - <<'PY' | tee "$HOME/spire-gpu-bench-$SLURM_JOB_ID.jsonl"
import json
import time
import torch
import spire_gpu
from benchmark_cuda import measure

assert torch.cuda.device_count() == 1, 'Slurm must expose exactly one GPU'
torch.set_num_threads(1)
torch.cuda.set_device(0)
print(json.dumps(dict(
    gpu=torch.cuda.get_device_name(), torch=torch.__version__,
    cuda=torch.version.cuda, module=spire_gpu.__file__,
    engine_commit='52d6257', benchmark_commit='52d6257',
    mode='random', minimum_batches=400, minimum_seconds=5,
    warmup_batches=10,
)), flush=True)
for size in (32, 128, 512, 2048, 8192, 32768):
    print(json.dumps(dict(stage='start', batch_size=size,
                          unix_time=time.time())), flush=True)
    measure(size, 5, None)
PY
```

提交作业：

```bash
cd "$HOME"
sbatch "$HOME/spire-gpu-benchmark.sbatch"
```

保留 Slurm 设置的 `CUDA_VISIBLE_DEVICES`。`measure(..., None)` 对应本报告的随机工作负载；直接执行 `tools/benchmark_cuda.py` 的完整入口还会准备 CPU 深层路线，因此复现仅 GPU 随机测速时使用上述导入方式。

## 数据溯源

本报告在测速结束后添加，性能数据对应固定提交 `52d6257`，而非文档提交或仓库后续 `main` 版本。原始 JSONL 按文件字节原样保存，其 SHA256 为：

```text
44644ef95d6e0d217409eb1de1eb64cd1b971349d0843bf505b77a38064d42b7
```

原构建日志标识为 `build-split-52d6257-3496.log`，SHA256 为：

```text
272703111747fd000edae111a4fd5538b247fe5109c4e1be93261f6f4e81d5dc
```

本地留档还包括完整测速日志、构建日志、模块清单、原作业脚本、memcheck 摘要和 union PTX 对比。仓库中的 JSONL 可直接核对本文的表格和累计值。
