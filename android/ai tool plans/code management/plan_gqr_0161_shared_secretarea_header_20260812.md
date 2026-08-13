# GQR-0161 shared secretarea header follow-up

## Goal

Move the byte-identical, branch-added D1 and D2 `secretarea.h` interfaces into the
shared source directory so the shared `secretarea.c` implementation and all
consumers use one declaration owner.

## Plan

- [x] Verify provenance, byte identity, consumers, and current worktree scope
- [x] Add `android/app/src/main/cpp/shared/secretarea.h` and remove both copies
- [x] Update naming and serialization contract tests for the shared ownership
- [x] Confirm include paths and build registrations for D1, D2, Android, and tests
- [x] Run focused contracts, scoped quality, Windows D1/D2 builds, and Android native build
- [x] Update the canonical ledger and record final diff metrics

## Result

- Removed `d1/main/secretarea.h` and `d2/main/secretarea.h`, each 42 lines
- Added one 45-line `android/app/src/main/cpp/shared/secretarea.h`
- Combined with the serialization extraction, GQR-0161 removes 122 lines from
  D1/D2 branch paths: 38 original `state.c` additions and 84 duplicated header
  lines
- All include spellings remain unchanged and use existing shared include paths
- 28 focused contracts, Windows D1/D2, and Android arm64-v8a,
  armeabi-v7a, and x86_64 builds passed

## Starting state

- HEAD: `4891e3dd6decae1e679eaa2a18ea923ab7a5b3f4`
- Both headers are byte-identical and first appear in branch commits dated
  2026-06-06
- The uncommitted GQR-0161 serialization extraction is preserved as the active
  product change being refined
