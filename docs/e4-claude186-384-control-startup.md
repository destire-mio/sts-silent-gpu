# E4 384 current-reward control startup

Slurm job 5435 started from source `d575609a06ad88418c79bfaf8e99928aec966e02` in a separate training directory after job 5434 stopped at a natural checkpoint. The 768 branch's dev-132, dev-134, dev-136 and final checkpoint remain in its original directory.

The control initialized from `dev-112.pt` and ran teacher-free 384 PPO with current rewards on two V100s. The setup used 2,048 lanes per GPU, rollout length 64, initial LR 4.44444444444e-05, and EMA decay 0.998. Auxiliary heads, arena, and warmup were off. Raw and EMA each completed the 1,024-seed baseline at zero training decisions. The next paired evaluation is scheduled for 7,200 training seconds.

| Update | Cumulative decisions | Global decisions/s | Peak allocated GiB (bytes) | Peak reserved GiB (bytes) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 262,144 | 2,714.71 | 3.54 (3,800,956,416) | 13.15 (14,122,221,568) |
| 2 | 524,288 | 2,703.63 | 3.75 (4,025,457,152) | 12.66 (13,595,836,416) |
| 3 | 786,432 | 2,658.80 | 3.79 (4,073,896,960) | 13.10 (14,067,695,616) |

The CUDA peaks are the maximum across ranks, reported separately for allocated and reserved memory. The GPU value calibration measured ratio **0.000951041715177** on rank 0 and applied coefficient **200** from update 1. The applied coefficient stayed at 200 through update 3.

The exact startup, baseline, calibration, and update rows were archived with SHA-256 `05fdea0f08db1dcae9fd4b77f58b07a2558d96cc9b9ee9335ad008f33ba6dedb`.
