# Claude #203 stage exams and offline combat distillation

The server recipe and its startup anomaly are recorded separately in `e4-claude203.md` and `e4-claude203-startup-anomaly.md`. The following tools do not activate combat distillation on the Slurm mainline.

## Fixed stage exams

```sh
python -m tools.e4_stage_exam dev-40-ema.pt --baseline dev-112.pt \
  --roots E2_TRAIN_ROOTS.jsonl.gz --output stage-exam --per-stage 256 --lanes 64
```

Each root has four matching whole-run determinization seeds. Play stops at the end of the current boss combat. The report includes win rate and root-level SE, HP lost, potions used, and candidate-minus-baseline paired differences. Full training checkpoints and plain raw/EMA snapshots are supported. Checkpoint hashes and exact root IDs protect chunked resume.

E2 prefixes replay with their original boss discovery profile 0. Old legal prefix actions are replayed directly through the engine; the evaluated policy does not rewrite them. Combat roots are extracted from recorded prefixes rather than from a model's ability to reach the boss. Both the source root and the exact derived combat root must be in the reserved hash split, so an identical late-training prefix is excluded.

**Data limitation:** the released E2 profile 0 has only one Act 3 boss before Act 4. A missing second-boss group is reported as unavailable, never as a zero win rate. The tool can also read a fixed `e4_stage_exam_roots_v1` root set with per-root `bosses_seen=511` for actual second-boss roots; different discovery profiles are evaluated separately. It does not alter the engine or fabricate these roots.

The independent server CPU exam uses a cap of 32 roots per stage and 32 lanes; its actual counts, paired results and wall time are archived separately when complete. CPU smoke results are not a GPU throughput estimate.

## Combat labels and measured budget

Run the measurement on the machine intended for label generation:

```sh
python -m tools.e4_combat_labels dev-112.pt --output combat-labels \
  --seeds 1000000000:1000004096 --pilot-seconds 1800 --max-seconds 10800
```

The 30-minute pilot is included in the three-hour total. After completed labels provide a measured rate, `pilot.json` records the chosen point budget; `summary.json` reports actual completed points, wall time, stage counts and paired correlation. Source episodes and label shards resume without relabeling committed roots. An interrupted active budget is conservatively charged through the next resume. No point count is inferred from a CPU smoke for a Colab GPU.

The frozen dev-112 policy captures boss/elite training decisions with at least two legal actions, at most four points per fight, stratified by act. Each point evaluates the top four actions on eight common random futures, then gives the surviving half sixteen additional matched futures. Argmax receives those additional futures even when eliminated, preserving the paired reference. Futures use `determinize_slots(..., True)` and stop after the current combat or 300 decisions. The target is V after combat, with death zero; capped or faulty futures are excluded rather than bootstrapped at the cap. Prefixes, observations, reference logits, per-action scores, paired deltas and SEs are retained.

## Standalone Colab fork

```sh
torchrun --standalone --nproc_per_node=1 -m tools.e4_stage12 \
  --output combat-control --init dev-112.pt --lanes 2048 --rollout-length 64 \
  --microbatch 256 --ema-decay 0.998 \
  --combat-distill combat-labels --combat-distill-weight 0
```

For the treatment, use a separate output directory and the chosen positive `--combat-distill-weight`; `--combat-distill-max-ratio` defaults to 0.5. A resume requires the same frozen label identity, weight and ratio. The flags are rejected on the Slurm mainline. Default weight is zero.

Targets use #202's paired-variance shrinkage and z clamp to +/-3. Unsearched legal actions keep z=0 and retain their original reference probabilities. Thus a root with no significant evidence preserves the full reference policy instead of pushing all probability onto the top-four subset.

Each PPO update samples 256 points and differentiates KL directly through the policy trunk. No head or stop-gradient is added. The separate KL gradient is added to the first successful PPO optimizer step of the update and capped against that step's PPO gradient before shared clipping and Adam. A checkpointed cursor prevents duplicate application after partial resume. Pools smaller than 256 are explicitly reported as sampling with replacement.

Logs include KL loss, gradient norms and ratios before/after the cap, combat PPO sampled K3 KL, combat policy entropy in nats and normalized units, and the fraction of z with absolute value above 1. Combat PPO metrics are also collected in the explicit weight-zero control. Weight zero skips label sampling, model forward/backward and RNG calls; the unchanged policy and Adam path is checked bitwise against the published pre-combat implementation.

Verification: 14 native CPU tests passed in 15.47 seconds. A separate real Gloo DDP execution also passed the parameter/Adam/RNG bitwise comparison, positive trunk update and gradient cap. The published pre-combat PPO reference SHA256 is `d96ece2503a1918144817a99531f1208064c7ccef05e9bfdbca1451b36f96c0c`.

A native CPU label smoke actually completed two points (one each from Acts 1 and 2), 6583 preview decisions, in 273.414825 wall seconds. The Act 1 paired correlation was 0.388345358; the Act 2 returns had zero variance, so its correlation is unavailable. This is a two-point correctness smoke with a shortened pilot threshold, **not** the requested 30-minute target-hardware pilot or a proposed Colab point budget. Those measurements, and production first-three-update health, remain pending.
