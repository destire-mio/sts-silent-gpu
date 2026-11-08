# Colab learner speed options

The standalone Stage12 entry accepts two opt-in flags in commit `afedf7e3acd9613204cc0620f453f5e0d38a45c0`. It keeps the 64-step default and uses the existing PPO learner.

- `--compile` compiles the encoder layers with dynamic shapes enabled during learner forwards. Rollout and evaluation use the eager forwards. Checkpoint parameter names stay the same.
- `--fused-sdpa` allows flash and memory-efficient SDPA during learner forwards. If neither kernel accepts the device, dtype or mask, the learner reports the rejection and falls back to PyTorch's default SDPA selection. The benchmark reports the observed backend for a representative training forward. Compiled graphs may hide the individual backend event.
- The learner already buckets the local examples in each global minibatch by entity count in groups of 16, then orders by candidate count before splitting microbatches. Both benchmark and training reuse that path.
- `tools/colab_speed.py` now sets the encoder's inner autocast dtype through `encoder_dtype_override`.

Run the benchmark from the repository root in the Colab environment with the engine on `PYTHONPATH`.

```bash
python -m tools.colab_learner_bench --checkpoint /content/dev-132.pt --width 768 \
  --lanes 512 --rollout-length 64 --microbatch 512 --dtype fp16 --recompute off
```

The script collects a fixed-seed batch once. Each combination starts from the same model, samples and minibatch RNG. It times one complete forward, backward and Adam update, including first-use compilation where applicable. It returns a nonzero status when the valid example counts differ, AMP skips an update, or the loss or unclipped gradient norm differs from the baseline by 0.001 or more in relative terms.

To enable either flag on the standalone training command, append it to the existing arguments. For example:

```bash
torchrun --standalone --nproc_per_node=1 -m tools.e4_stage12 \
  --init /content/dev-132.pt --output /content/drive/MyDrive/e4 \
  --lanes 4096 --microbatch 512 --recompute off --dtype fp16 --rollout-length 64 \
  --compile --fused-sdpa
```

The isolated Linux CPU run passed the new learner smoke and the existing standalone checkpoint restart test. The new smoke exercises dynamic capture with Dynamo's eager backend. Two tests passed in 31.17 seconds, with a NumPy writability warning and a DDP deprecation warning. CUDA timing, the actual G4 backend, and Inductor performance await the Colab run. The server uses its separate runtime.
