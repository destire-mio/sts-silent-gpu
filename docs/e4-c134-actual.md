# Claude 134: observed four-pool training

The prepared Act3-start pool ran in the natural continuation. All eight ranks logged restoration at update 996, with no fresh-collector fallback. Initial per-rank input hashes were not captured during the connection outage, so these logs do not establish byte-for-byte identity of all historical inputs.

Actual PPO update 997 was finite with pool sizes 256/256/256/0; update 998 was finite with 256/256/256/56. The fourth pool reached 256 by update 1001. The verified collector source uses 45/10/20/15/10 fresh/Act1-boss/Act2-start/Act2-boss/Act3-start, with each optional pool below 32 routing its share to Act2-start. The startup `midstart_policy` line still prints the older static three-pool labels; it is not evidence of the executed selection thresholds.

The later natural save is update 1043, serial 123, at 199399.3981608016 cumulative training seconds (55.3887217113 hours). All eight current rank files match that generation and retain 2048 lanes, four pools of 256, RewardLedger state, collector RNG, and Python/NumPy/Torch/CUDA RNG. Shared Adam is 66778, including 49 already-applied partial steps; the completed-round log count 66729 must not replace it.

The last completed update is finite. The allocation saved with `allocation_end_or_yield`; CUDA allocator OOM warnings remain in the logs, and this does not establish zero OOM events. The latest completed fixed 1024-game argmax evaluation remains 54h. The next scheduled evaluation is 56h, at 201600 cumulative training seconds. No new evaluation, GPU lookahead, or 512-model training was launched for this verification.
