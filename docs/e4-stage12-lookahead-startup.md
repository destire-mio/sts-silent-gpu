# E4 startup failure after the 60-hour evaluation

Job 5617 completed normally and saved update 1868 at 220867.84561298264 training seconds and 489,684,922 decisions. The 60-hour raw and EMA evaluations had already completed and been audited at update 1828.

Jobs 5628 and 5631 both loaded update 1868 on two V100s. The saved `dev-60.pt` triggered the legacy #137 lookahead supplement during startup. The supplement requires eight ranks and asserted `world == 8` on both ranks before another training update could complete. Slurm recorded FAILED 1:0 for both jobs. Their batch peak RSS was below the first job's host-memory request, and the worker tracebacks identify the rank assertion.

The original controller recorded 6963 elapsed seconds for 5617, 302 for 5628 and 297 for 5631. At two ranks the respective charges are 1740.75, 75.50 and 74.25 node-s, bringing the settled ledger to 429332 node-s at this observation. The successor guard was set at 08:56:32 UTC. At that observation 5632 was running and 5633 was already queued behind it.

The next ordinary raw and EMA evaluations remain due at 223200 training seconds. Recovery uses the natural update 1868 checkpoint and must keep the legacy eight-rank supplement out of the small-QoS training path.
