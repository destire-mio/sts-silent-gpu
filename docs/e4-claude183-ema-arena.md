# E4 #183 · standalone EMA and hard-fight practice

Both options default to zero on fresh standalone runs. For example:

```bash
torchrun --standalone --nproc_per_node=1 tools/e4_stage12.py \
  --init dev-132.pt --output training-practice \
  --ema-decay 0.998 --arena-fraction 0.125
```

EMA updates once after each applied optimizer step and saves in `model.pt`.
Ordinary evaluations use the same fresh seeds and full-run argmax procedure
for both weight sets. Each evaluation records its `weight_source` and keeps its
own episode and ledger files. Raw weights supply the ordinary dev snapshot
and the existing evaluation gates.

The arena stores the last 512 entries of each of six categories, captured
before the first combat decision of a fresh run's qualifying fight. The
configured fraction reserves training lanes once any pool is populated. When
elite and boss pools both contain entries, boss categories get 75% of draws,
with Act 2 Boss twice as likely as either other boss category. Practice starts
use the native determinizer. A win keeps the engine's normal reward and
bootstraps detached run value at the combat exit. Death and smoke end the
practice segment with zero terminal reward. Per-category starts, finished
fights, wins, win rates, and faults appear in the training update JSONL.

Resume from the same output directory. Omitted options inherit the checkpoint
settings. Arena pools, live fights, and pending randomization survive rank
checkpoint replay. The fraction can be adjusted while practice stays enabled.

## CPU validation

An explicit zero-decay, zero-arena run matched the prior standalone runtime
bitwise after one full update, including model and optimizer tensors,
sampler RNG, and recorded lane paths. The checkpoint key sets matched.

The dev-132 smoke warmed the pool through the normal capture path using
known seed 2000000572. With fraction 0.5 and decay 0.998, it completed one
update and one resumed update with two applied optimizer steps each. It
observed one Act 2 elite win in the first update and began a determinized
Act 2 Boss fight after resume. The boss fight was still open at the second
checkpoint. A separate one-seed CPU evaluation wrote both raw and EMA
results through the full-game argmax path.
