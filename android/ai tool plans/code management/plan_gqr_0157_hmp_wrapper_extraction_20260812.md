# GQR-0157 HMP wrapper extraction plan - 2026-08-12

## Objective

Finish the existing shared HMP-to-MIDI extraction by moving the identical
Android wrapper and tempo payload out of inherited `d1/misc/hmp.c` and
`d2/misc/hmp.c`. Preserve each game's allocator and linkage contract while
reducing the branch-caused inherited diff by the measured 24 lines and four
hunks.

## Plan

- [x] Freeze the current paired HMP residue, shared converter API, callers,
  tests, build ownership, and merge-base metrics
- [x] Add the public `hmp2mid_mem` wrapper and canonical tempo payload to the
  branch-added shared HMP owner
- [x] Remove the duplicated inherited includes and wrappers while preserving the
  original per-game tempo arrays used by the inherited file conversion path,
  preserving declarations, allocation ownership, and exact MIDI output
- [x] Add or extend focused contracts for D1/D2 API parity and exact successful
  output bytes
- [x] Run scoped quality, focused HMP tests, Windows D1/D2 builds, Android ABI
  builds, and final inherited-diff accounting
- [x] Mark `GQR-0157` done and `GQF-0170` fixed with terminal evidence in the
  canonical ledger
