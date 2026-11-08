# E4 PPO value calibration on forced actions

## Server measurement

Job 5431 saved update 62 after distillation and spent its remaining allocation on the deferred supplemental evaluation. Its log contains no value recalibration event. The natural successor, job 5432, measured the first PPO update using c196. This is the original log line.

```json
{"event": "claude171_value_recalibrated", "start_update": 63, "old_coefficient": 181.3911719048255, "new_coefficient": 76.96145098000254, "ratio": 0.0032483795044997185, "milestones": {}}
```

Update 63 completed at training clock 476247.40247325954 seconds with 262,144 valid decisions, 16 attempted and applied optimizer steps, zero AMP skips, and zero episode faults. It used the inherited coefficient 181.3911719048255 and recorded first-round value MSE 0.00030730400631981446. The server measurement was finite, so the server kept its current calibration and planned ramp.

## Reproduction and repair

The old diagnostic used the leading microbatch after sorting the local samples into length buckets. With a single legal action, its log probability is constant, so the actor term contributes zero encoder gradient. The value term can still have nonzero gradient.

A CPU FP32 replay from `dev-132.pt` collected 384 valid decisions using the standalone seeds and a smaller 32-lane, 12-step rollout. Of those decisions, 90 were already forced and the policy mask made another 28 forced. The old sorted prefix contained 16 out of 16 already-forced decisions. Its actor norm was 0, the ratio was infinite, and the target was 0.5. In the same fragment, 16 nonforced decisions produced actor norm 1.1367034766031683, ratio 0.00021283271172070297, and target 200. The nonforced measurement matched at scales 1 and 65536 in FP32. The original G4 batch is unavailable, so its exact forced-action count and FP16 behavior remain unverified.

Commit `e2b20df18901a6ccaff8ff0913220daa01da90e2` chooses nonforced rows with nonzero advantages for #171 and checks the effective legal mask after unpacking. It retains the current coefficient and retries when the actor norm is zero or the measurement is nonfinite. A saved infinite calibration also re-enters this measurement on standalone resume.

To align Colab at a chosen point in the server's ramp, append `--value-coefficient 181.3911719048255` to the existing standalone resume command to use the server's first-round coefficient. The flag accepts a finite nonnegative value. The fixed coefficient is stored in the checkpoint and takes precedence over automatic calibration on future resumes.

The isolated CPU run passed six tests, including the forced-to-nonforced calibration regression and a standalone restart check that persisted the manual coefficient. No new GPU calibration run was used.
