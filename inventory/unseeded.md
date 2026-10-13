# Unseeded combat calls

| Java method | Call | Gameplay effect | Engine representation |
| --- | --- | --- | --- |
| `actions.common.DiscardAtEndOfTurnAction.update` | `Collections.shuffle(cards)` | Orders end-of-turn card hooks; ethereal exhaust actions enter the queue at its front, reversing that order. Retained cards have been removed before this shuffle. | `Game.unseeded`, a persistent `java.util.Random` port seeded by scenario `unseeded_seed` (default `0`). Its state appears in snapshots. |

Java's no-argument `Collections.shuffle` uses a shared, unseeded Java RNG. The engine's pin must be agreed with the oracle before traces can establish equivalence. The recorder currently seeds a new `java.util.Random` from its shared `RandomXS128` stream for each shuffle; that is a different pin.

Visual-only random calls have no engine state. The oracle must keep visual randomness from changing the pinned gameplay sequence.
