# Counterstrike cross-level route regression audit

## Goal

Identify why recent shared GuideBot routing changes fix one Counterstrike level
while breaking previously confirmed levels, beginning with the level 2 blue-door
stall, and replace the oscillating behavior with tested cross-level invariants.

## Plan

1. Audit the complete unstaged route-planning, mission metadata, and simulation
   diff against `HEAD`, including every Counterstrike status and route change.
2. Reproduce Counterstrike level 2 with the current Windows route engine and
   capture the selected objectives, held keys, physical frontier, and door state
   at the stall.
3. Map each observed regression to the shared planner or certifier rule that
   caused it, and distinguish metadata-order failures from live-navigation
   failures.
4. Define invariants for keyed doors, asymmetric auto-closing doors, static-key
   excursions, and current-state path certification, then encode focused unit
   tests plus a multi-level Counterstrike regression gate.
5. Implement the smallest invariant-based correction, regenerate only affected
   metadata and simulation entries, and verify levels 2, 6, 11, and 12 together.
6. Run the Windows D2 build, native tests, scoped quality checks, and record the
   final regression matrix here.

## Status

- [x] Diff and regression matrix audited
- [x] Level 2 reproduced and diagnosed
- [x] Cross-level invariants and tests added
- [x] Shared fix implemented
- [x] Affected metadata and simulations regenerated
- [x] Build, native tests, and multi-level route gate passed

## Findings

- The bad corpus contained 47 `ok` to `timeout` transitions. Most reported a
  physical frontier that did not open; equivalent reused levels reported a
  no-progress timeout.
- Counterstrike level 2 retained the same route-input hash, proving that its
  failure was in live certification rather than route generation.
- Commit `f781b45e` rejected a currently flyable side whenever its wall retained
  `WALL_DOOR_LOCKED`. That flag is insufficient to predict whether the open
  portal will later become unusable.
- Static route planning already owns the stable-return decision needed by
  Counterstrike level 12. Live physical and progress reachability must accept
  the current engine fact that the side is flyable.
- Removing the live heuristic restored Counterstrike levels 2, 4, 6, 10, 11,
  and 12 in deterministic repeated runs. Cross-mission checks restored Chasm
  level 1, Belial level 3, and TEW level 4.
- All 47 known-bad corpus rows were rerun twice and written incrementally. No
  `ok` to failed, partial, or timeout transition remains in the unstaged
  simulation diff.

## Final validation

- Windows D2 build passed.
- All 45 native D2 CTest tests passed.
- Counterstrike levels 2, 4, 6, 10, 11, and 12 confirmed twice with identical
  frames and ending simulation RNG per level.
- Chasm level 1, Belial level 3, and TEW level 4 confirmed twice with identical
  frames and ending simulation RNG per level.
- Scoped clang-format and PSScriptAnalyzer checks passed.
