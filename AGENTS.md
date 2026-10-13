# Rules

Read `docs/design.md` before writing code.

## Ground truth
- The original game decides what is correct. Decompiled source (read-only, never copy it or any
  game asset into this repo):
  `/Users/destire/Documents/Codex/2026-09-10/new-chat-2/work/native-complete-source/source`
- Port the Java behaviour, including its quirks and its order of operations. Do not "fix" the game,
  do not approximate, do not port from memory of the game or from wikis.
- A unit is done when its oracle traces replay with zero differences. Your own report does not count.
- Recorder driving and observation code may be fixed when the Java and a reproduction show
  that it misobserves. The 18 patched game classes, the RNG split and the comparison must stay
  unchanged. Traces may only be re-recorded from the original with the same seed and scenario,
  never edited.

## Code
- No defensive programming. No fallback paths, no try/catch around things that should work, no
  validation of data the engine itself produced, no compatibility shims, no feature flags, no
  optional modes. One way to do each thing. Broken invariants abort loudly (`SPIRE_ASSERT`).
- No speculative generality: no abstraction with a single user, no hooks for other characters.
- No dead code, no commented-out code, no `_v2` / `_old` / `_new` copies. Replace in place.
- Comments only where the Java behaviour is surprising; cite the Java class and method.
- Match the style already in the file.

## Repo hygiene
- Commit only source, build files, the inventory and small tests. No reports, progress notes,
  plans, logs, benchmark dumps, build output, screenshots or scratch scripts. If you need scratch
  space use a temp directory outside the repo.
- No new top-level directories or Markdown files without a design reason stated in the commit.
- Large traces are generated, not committed (`traces/` is ignored). Tests commit only small
  hand-picked traces under `tests/traces/`.
- Small commits on your own branch, imperative English subject, one logical change each.
  Do not push, do not open pull requests, do not rewrite history.

## Machine
- Never change proxy, DNS, VPN, package mirrors or Docker state. If a download is slow or fails,
  stop and report it.
- Never modify the Steam install, its saves, or anything under
  `/Users/destire/Documents/Codex` and `/Users/destire/Documents/ChatGPT/sljt` outside your own
  working copy. Those are read-only references.
- At most 3 CPU-heavy processes at once; other agents share this 10-core, 16 GB machine.
