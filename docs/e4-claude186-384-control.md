# E4 384 current-reward server control

Decision #186 starts a fresh teacher-free PPO group from the audited `dev-112.pt` weights in a separate Slurm output root. The preparation command takes `--init` and produces a zero-update, zero-Adam-step checkpoint. The controller launches the usual Stage12 worker from this checkpoint and retains its current allocation and settlement rules.

The control uses rollout length 64, 2,048 lanes per GPU, the existing 4.44e-5 learning-rate cap, and EMA decay 0.998. The initial optimizer update uses the coefficient obtained with the #179 nonforced-sample gradient measurement. The reward ledger is `current`. The group has no teacher, auxiliary heads, arena practice, or value warmup.

The worker records actual completed-round decisions and peak allocated and reserved CUDA bytes across ranks. Every two training hours, the 1,024-seed argmax evaluations of raw and EMA weights carry the same cumulative decision count and separate snapshots. The corresponding Slurm startup measurements are reported separately once the first three rounds finish.
