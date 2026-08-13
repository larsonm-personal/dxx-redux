# GQR-0159 mixer diagnostics consolidation plan - 2026-08-12

## Objective

Move the duplicated Android SDL_mixer initialization queries and diagnostics
from inherited D1/D2 sources into their branch-added shared owner. Preserve the
requested rate and buffer inputs, exact messages, actual device reporting, and
desktop behavior while minimizing the inherited diff.

## Plan

- [x] Freeze the current worktree, paired mixer residue, shared API, build
  ownership, tests, and merge-base metrics
- [x] Narrow the shared API so it owns actual-spec queries and success/failure
  diagnostics
- [x] Reduce both inherited mixer initialization paths to compact paired calls
  without changing initialization or teardown order
- [x] Add focused contracts for exact diagnostics, query ownership, API parity,
  and retained requested-rate/buffer arguments
- [x] Run scoped quality, focused tests, Windows D1/D2 builds, Android ABI
  builds, and final inherited-diff accounting
- [x] Mark `GQR-0159` done and `GQF-0172` fixed with terminal evidence in the
  canonical ledger
