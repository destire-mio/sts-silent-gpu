# E4 #181 and #182 · standalone PPO auxiliary heads

Enable either head with `--combat-aux-weight W` or `--global-aux-weight W` on
`tools/e4_stage12.py`. On a fresh run, both default to zero. For example:

```bash
torchrun --standalone --nproc_per_node=1 tools/e4_stage12.py \
  --output training-aux --init dev-132.pt \
  --combat-aux-weight 0.2 --global-aux-weight 0.3
```

Resume in the same output directory with the same lane, rollout, precision,
and learner settings. Omitted weights inherit the saved values. Positive weights
can be adjusted on resume with the same enabled heads. `model.pt` stores the
heads and their Adam group, and rank checkpoints store unfinished targets and
fresh-lane calibration cohorts. Ordinary `dev-*.pt` exports retain the policy
weight format. The switches are available in standalone PPO. Sampling and
evaluation still use the policy logits and run value.

The combat head predicts the observed win and remaining HP fraction for fresh
and midstart fights. Death contributes zero HP. The global head predicts Act 1
Boss, Act 2 Boss, Act 3 first Boss, Act 4 entry, Heart victory, and terminal
floor divided by 55. A milestone already reached is masked for that decision.
Open trajectories use detached window-end probabilities and the PPO GAE
lambda (0.995). The actual outcome supersedes the bootstrap when it becomes
available in the window.

The update JSONL contains `auxiliary.combat` and/or `auxiliary.global`. BCE and
regression errors include bootstrap targets. `brier` and combat `auc` use
observed outcomes, while `target_brier` includes bootstrap targets. Missing
observations, including single-class AUC, yield null. Fresh calibration reports
the mean first-decision prediction for starts in this update, and reports
paired first-decision predictions and hit rates for fresh episodes that
completed in this update.

Stage6's `CombatPolicy.combat_value` estimated the whole-run value at the end of
a copied combat branch, with zero on death. Stage12 kept the policy and
whole-run value for its no-search PPO objective.

## CPU validation

Four tests passed in 140.62 seconds on the Linux standalone runtime. Both
weights zero produced bitwise-identical model tensors, Adam state, learner
cursor, and PPO RNG after one learner step against the prior implementation.
The real 768×12 dev-132 snapshot completed an update with weights 0.2 and 0.3,
saved, and completed a second update after restart. The two rounds each
applied two optimizer steps, with both heads' Adam counters advancing from 2
to 4. The first update trained on 53 of 53 combat decisions, including 26
bootstrap labels. The second trained on 33 of 33. The two fresh games were
still open at the end of the smoke, so the observed milestone hit rates
and Brier scores were unavailable in those windows.
