# E4 Colab PPO-transition weights

The natural 132-hour evaluation at update 62 scored 31.328125 on 1,024 argmax seeds and passed the 30.96 gate. Job 5431 saved `dev-132.pt` and emitted `distillation_complete`. The [132-hour evaluation report](e4-evaluation-132h.md) records the independent CPU audit.

- [Download branch](https://github.com/destire-mio/sts-silent-gpu/tree/colab-weights)
- Orphan commit `73a15dda66739f4537da96e73336189da1c072af`
- Tree `1dea26cd22aac39a154f5da9984e6fd705df9eb5`
- Asset `dev-132.pt`, 361658543 bytes, 8 parts of at most 45 MiB
- SHA-256 `d889dae3291ea0fdcf3ec091bbb3eb678929f4b47ac5cc2177816d7d5cdd4019`

The cluster verified the original weight-only file by strict CPU load and reassembled the parts to the same SHA-256. The branch contains the eight parts and `SHA256SUMS`.

```bash
git clone --depth 1 --single-branch --branch colab-weights https://github.com/destire-mio/sts-silent-gpu.git e4-colab-weights
cd e4-colab-weights
cat dev-132.pt.part-[0-9][0-9] > dev-132.pt
sha256sum -c SHA256SUMS
```

The first cluster upload stalled after GitHub acknowledged 254,406 bytes on port 22. The cluster retried on `ssh.github.com:443` and completed in 327.83 seconds. The branch head, all nine blob IDs and sizes, and the exact manifest matched. Both temporary deployment keys were revoked and their private key files were removed. [GitHub SSH port documentation](https://docs.github.com/en/authentication/troubleshooting-ssh/using-ssh-over-the-https-port)
