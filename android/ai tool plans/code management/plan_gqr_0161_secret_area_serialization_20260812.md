# GQR-0161 secret-area serialization extraction plan - 2026-08-12

## Objective

Move the paired Android secret-area save and restore serialization bodies from
inherited D1/D2 `state.c` into the existing per-game compiled secret-area
adapter. Preserve the exact bytes, version gates, endian conversion, count
mismatch fallback, and save/restore ordering while reducing the inherited diff.

## Plan

- [x] Freeze the clean worktree, paired serialization bodies, adapter API,
  compilation ownership, tests, and merge-base metrics
- [x] Move the writer and reader implementations plus necessary declarations to
  the branch-added adapter owner
- [x] Remove paired inherited bodies while preserving the existing call sites
  and exact file-position ordering
- [x] Add focused serialization contracts for D1/D2 parity, byte layout,
  version/endian behavior, and count-mismatch fallback
- [x] Run scoped quality, focused tests, Windows D1/D2 builds, Android ABI
  builds, and final inherited-diff accounting
- [x] Mark `GQR-0161` done and `GQF-0174` fixed with terminal evidence in the
  canonical ledger

## Shared-header follow-up

- [x] Replaced the byte-identical, branch-added D1/D2 `secretarea.h` files with
  the matching shared header beside `secretarea.c`
- [x] Preserved all include spellings through existing shared include paths
- [x] Revalidated focused contracts, Windows D1/D2, and all Android ABIs
