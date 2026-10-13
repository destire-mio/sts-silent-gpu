# sts-silent-gpu

An exact reimplementation of Slay the Spire 1 for the Silent (Ascension 20, three keys, Heart),
written so that tens of thousands of games advance in parallel on one GPU.

Same seed and same action sequence must give the same game as the original Java build
`[V2.3.4] (12-18-2022)` at every decision point. The original game is the only judge.

- `core/` engine, restricted C++ that compiles for CPU and for CUDA
- `host/` CPU batch driver and Python binding
- `cuda/` GPU kernels (built only where `nvcc` exists)
- `oracle/` harness that drives the original game and records traces
- `inventory/` list of every game element in scope and where its Java source is
- `tests/` replays oracle traces through the engine and compares snapshots
- `docs/design.md` architecture and the trace/snapshot contract

Rules for contributors, human or agent: `AGENTS.md`.
