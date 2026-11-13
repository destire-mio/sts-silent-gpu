# E4 return to the #203 recipe

The original controller has bound the existing job 5635 to the #203 recipe and runtime `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`, which includes the two-card lookahead fix and logs the skipped supplement. Job 5635 will resume from 5634's latest natural checkpoint. It requests the existing 4744 MiB and two V100s.

All 21 faults in 5634's first resumed update were the engine's `InvalidCandidate` code (`0xe0000004`). The engine raises it when an action index is outside the current legal candidate list. The faults involved 17 distinct seeds, all in E2 late starts: 10 Act 4 entry, 6 Act 3 boss, 3 Heart start and 2 Heart mid. Update 1869 completed with 16 optimizer steps and zero AMP skips. Updates 1870–1879 recorded zero episode faults while still using #186. These logs cannot establish whether the initial faults were specific to the old runtime or why the indices were out of range.

For future failures, repair the fault and resume the current recipe. If recovery requires a rollback, publish the reason before switching.
