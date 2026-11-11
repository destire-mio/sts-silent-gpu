# Claude #203 startup anomaly and rollback

The recipe source was published at `7c75b76ecc46bdca4559c1edc4204fca384ee26f`. Eight native CPU tests and a real checkpoint smoke through updates 1–3 passed, preserving Adam and EMA. No production update with the new recipe completed.

The previous control's actual entropy coefficient was **0.01**; #203 requests 0.005. Worker 5496 ended at its native safe boundary with update **1311**, **153830.01948512695 training seconds**, and **343670757 cumulative training decisions**. Model, rank checkpoints, and `dev-40-ema.pt` are preserved.

## Observed failure

The launcher initially omitted the physical `train/e4_checkpoint.py` path used by the original controller before worker startup. Job 5497 failed in 6 seconds. The launcher's train symlink and sbatch import path were repaired; GPU, CPU, memory and wall-time arguments were left untouched. One original-controller recovery enqueue created 5498.

The original controller's unchanged settlement logic used the failed startup's small host-memory peak to lower the following request to **822 MiB**. Jobs **5498–5520** then ended as actual Slurm `OUT_OF_MEMORY`, including original-source jobs after the source rollback. Subsequent jobs were automatic original-controller successors. These failures do **not** measure the new recipe's normal GPU or host-memory demand.

The source was rolled back to `d575609a06ad88418c79bfaf8e99928aec966e02` at 2026-11-11 01:17:17 UTC. The original `halted` successor guard was set at 01:24:50 UTC to end the repeated failure sequence. No jobs were cancelled, signalled or force-checkpointed. The scoped queue is empty.

At 01:27 UTC, canonical accounting still lacked 5520 settlement. A subsequent original-controller reconciliation settled its actual 36 seconds at world 2 as 9 node-seconds once. The canonical total is now **408801.5 node-seconds**. Jobs 5497–5520 together cost **182.25 node-seconds**. Job 4549 remains unsettled; no refund was invented. Job 4557 remains held and guard 4597 remains off.

The canonical controller SHA256 remains `bebd0a09df34ecaf2f8a3bf55735c79a4914f9e02a8f1f0ef7be97564ae47269`. No coordinator watch or additional global resource query was used.

## Recovery boundary

A reviewed recovery script is prepared but **not applied**, pending the direct user's exception to the explicit no-manual-resources constraint. It would use the original formula on completed job 5496's real aggregate peak, `ceil(6200340480 * 1.25 / 1048576) = 7392 MiB`, then recover the original recipe through one original-controller enqueue. Controller source and other resource settings stay at their recorded values. The pending question must not be treated as approval.

The first three actual new-recipe health measurements remain pending. The ordinary 42h raw/EMA pair remains published at `e4334d2b34b456e7a38f5fea8ac42f2d3c7a8b0e`; the next training threshold is 158400 seconds. Stage-exam native checks and independent CPU execution are progressing; combat distillation remains an offline, default-off Colab fork. #204 stays queued after #203.

Raw receipts, failed-job logs, checkpoint hardlink backups and CPU evidence remain in `c214-claude203-preparation` on the server, with local coordinator evidence and handoff records.
