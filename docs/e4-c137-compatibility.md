### C137 evaluation compatibility correction

The dev-60 first-256 comparison completed its timed GPU argmax baseline (53.340614850996644 seconds), then the turn preview failed on a CUDA UInt32 advanced-index operation while checking native fault flags. Turn and combat results are incomplete; no comparison or speedup is claimed.

The correction reinterprets the native four-byte fault flags as int32 before selecting preview slots. It preserves every nonzero bit, including high bits. Candidate selection, rollout depth, scoring, masks, model, training reward, learner and evaluation-window scheduling remain unchanged. A new CPU regression checks sparse selection, high-bit faults, empty selection and unchanged source storage; 1 passed in 2.11s. No GPU benchmark was added.

The completed argmax seed receipts and timing remain retained. The corrected source is prepared for natural continuation; it does not interrupt an allocation or rerun the completed baseline. The comparison will only be reported after complete same-checkpoint seed coverage, or an explicit permitted combat skip.

The UInt32 correction was exercised in the next natural window, where turn previews advanced and 21 completed games were persisted. A separate heartbeat diagnostic then raised KeyError while treating a cached preview slot as a mainline lane with provenance. Turn remains incomplete; the completed argmax baseline is unchanged.

The second correction presents only mainline lanes to the existing heartbeat and restores the entire preview cache in a finally block. Candidate scoring and the CombatLookahead class are unchanged from the UInt32 correction; existing deadline and stop exceptions still propagate. Two new CPU regressions exercise the real mainline heartbeat and cache restoration when the existing clock stops. 2 passed in 2.11s. No GPU test or extra allocation was started.

The failed attempts retain their real allocation charges. Exact failed-turn mode durations were not saved before the exceptions, so those durations cannot be claimed as complete measurements; the successful-baseline timing is independently preserved. Future completed-mode timing must be distinguished from this missing failed-attempt timing.
