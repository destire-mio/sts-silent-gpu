# Claude #203 mainline recipe

The prior 384 control used an entropy coefficient of 0.01. The new recipe uses 0.005, strategic share 0.35, late pool share 0.2 and key-last-chance masks. It resumes the current checkpoint with its Adam state, EMA, update and training clock.

The strategic weights apply after global advantage normalization. Counts include valid nonforced decisions across ranks. Card reward, map, shop and campfire decisions receive q/p, and the other nonforced decisions receive (1-q)/(1-p). Forced decisions keep weight 1. An absent group keeps weights 1 and is visible in the logged counts.

The E2 roots use their original boss discovery profile during replay. A stable hash of the source seed and exact action prefix reserves 20% of roots for stage exams. The late pool loader excludes these roots. Existing Act 3 episodes defer a newly enabled key mask until their next start.

Controller worker arguments

~~~sh
--strategic-share 0.35 --entropy-coefficient 0.005 --late-pools --late-pool-fraction 0.2 --late-pool-holdout-fraction 0.2 --late-pool-roots E2_TRAIN_ROOTS --key-last-chance
~~~

Each update logs the strategic share before weighting, both weights, global decision counts, allocated/reserved peaks by rank, entropy, PPO KL, clip fraction, value loss and actual late start fraction. Ordinary 1024-seed raw and EMA exams include the key-mask status and the milestone chain.

Verification covered eight native CPU tests. A complete CPU checkpoint run resumed twice through the Slurm entry, reached update 3, retained Adam steps and EMA, and loaded the late pools with the held-out roots excluded. GPU health measurements are taken from the first three actual mainline updates after activation.
