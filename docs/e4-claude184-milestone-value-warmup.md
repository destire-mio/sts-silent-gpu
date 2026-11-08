# E4 #184 · standalone milestone rewards and value warmup

The default reward scheme is `current`, with zero warmup updates. A fresh
standalone PPO run can select the milestone ledger and a value warmup:

```bash
torchrun --standalone --nproc_per_node=1 tools/e4_stage12.py \
  --init dev-132.pt --output training-milestone \
  --reward-scheme milestone --value-warmup-updates 5
```

The milestone ledger divides earned points by 100. Floors award 0.1 each.
Act 1 Boss awards 1, Act 2 Boss 3, each Act 3 Boss 4, Act 4 entry 6,
Shield and Spear 5, and Heart 70. It shares the existing transition and
duplicate-reward checks. Midstart and arena snapshots carry the original
ledger and retain their reward accounting on resume.

Warmup collects new rollouts with the selected ledger and optimizes the
value head with unweighted MSE. Its Adam state starts empty. The encoder,
action head and optional auxiliary heads receive no gradient during
warmup. Update JSONL records the MSE and the per-act sample counts, signed
prediction-minus-target error, and MSE. The next PPO update schedules the
existing nonforced gradient measurement for the value coefficient. A fixed
`--value-coefficient` cannot be combined with this schedule.

Resume by using the same output directory. Omitted options inherit the
checkpoint's scheme and warmup length. Legacy checkpoints without a scheme
field use `current`. Use a new output directory with a weights snapshot
when changing the scheme or starting a new warmup.

## CPU validation

An explicit current/zero-warmup update matched the prior standalone runtime
bitwise in model, Adam, scaler, RNG, and lane trajectories. Checkpoint
key sets matched.

The actual dev-132 weights ran two value-only updates across a checkpoint
restart and resumed PPO on a third process. Each update applied two Adam
steps. The encoder and all non-value tensors matched the initial weights
during warmup. The resumed value Adam steps advanced from 2 to 4, then to
6 on the first PPO update. That update measured a new coefficient with the
existing nonforced gradient diagnostic. The short run sampled Act 1 only.
Warmup MSEs were 8.469787348116634e-06 and 0.00021702070778185922.
