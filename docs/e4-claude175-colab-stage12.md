# E4 #175 Colab Stage12 training

Implementation commit `0a0dbe96c0302c13aa17c7ff2e0af9c0413e9b47`. With the CUDA engine already built in `build/`, run from the repository root:

```bash
PYTHONPATH=build:. torchrun --standalone --nproc_per_node=1 -m tools.e4_stage12 \
  --init /content/dev-130.pt --teacher /content/dev-112.pt \
  --output /content/drive/MyDrive/e4-colab-stage12 \
  --lanes 2048 --microbatch 256 --recompute on --dtype fp16 --rollout-length 64
```

The 768-wide student starts from the supplied weights with a fresh optimizer and training clock. A teacher activates score-guided distillation. The existing 1,024-seed argmax evaluation and 30.96 PPO threshold apply. A fresh run without a teacher starts in PPO.

Use the same output directory and training flags on restart. An existing checkpoint takes precedence over `--init` and restores model, Adam, counters, RNGs and collection or learner progress. The worker checks the 30-minute save interval at safe rollout and learner boundaries. The standalone checkpoint retains the prior rank generation by copying, which avoids a hard-link requirement on the Drive mount.

One CPU smoke test ran two separate single-process workers. The first completed update 1 and saved Adam step 2. The restart read update 1, completed update 2 and saved Adam step 4. The isolated test initially stopped during import because the scratch directory lacked the engine card catalog. With the existing catalog available, pytest reported **1 passed in 24.42 s**, exit 0, with empty stderr. The smoke test covered the PPO checkpoint and restart path. Colab CUDA, Drive durability and the distillation-to-PPO transition await an actual Colab run.

The server training remains on its existing runtime. The two model files are still on the cluster and the `colab-1` upload is pending Windows transfer.
