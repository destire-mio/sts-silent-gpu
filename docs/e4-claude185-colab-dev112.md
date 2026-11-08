# E4 #185 · dev-112 for Colab

The `colab-weights` branch has dev-112 and dev-132 together at
`3b41b5242e637861fe3c9ff45661ae470a141978`. Dev-112 is 49,768,265 bytes, with SHA256
`5d6fc0ded5a10c522fc90a1844206109a205497f4c6b7c3f09ddb569828a9e45`.
Its two 45 MiB-capped chunks match the previously verified Git objects.
The manifest includes both reconstructed weights, and all eight dev-132
chunks retain their existing object IDs.

```bash
cat dev-112.pt.part-00 dev-112.pt.part-01 > dev-112.pt
grep '  dev-112.pt$' SHA256SUMS | sha256sum -c -
torchrun --standalone --nproc_per_node=1 tools/e4_stage12.py \
  --init dev-112.pt --output training-384 --microbatch 256
```

The unchanged standalone entry at `3eb9cfd278376627adf6aa225002ee5eee361aba`
strictly loaded all 94 dev-112 tensors. The 384×6 model ran teacher-free
PPO, checkpointed, and ran a second update after restart. The combined smoke
enabled both auxiliary heads, EMA 0.998, arena fraction 0.5, milestone rewards,
and one warmup update. It retained the warmup checkpoint after correcting
a test-only assumption about the value head's parameter names. The resumed
PPO update trained on 109 combat labels, logged all five global milestones,
used both heads' saved Adam moments, and remeasured the value coefficient. The
arena recorded a win in an Act 2 elite fight during warmup and two completed
Act 2 Boss fights after resume. Each of the four training updates applied
two steps with zero faults and AMP skips.

Start Colab at microbatch 256, the standalone default. The CPU smoke used
microbatch 16. Historical four-V100 job 5390 used 512 with a rank-zero peak
of 5,148,369,408 allocated and 11,569,987,584 reserved bytes on update 3008.
These are historical server measurements. Colab device capacity and enabled
features determine the usable microbatch.
