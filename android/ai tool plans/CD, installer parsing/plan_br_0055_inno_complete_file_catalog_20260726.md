# BR-0055 complete Inno file catalogs

## Goal

Ensure a successful Inno archive open exposes every declared file entry, with
checked heap-backed storage and explicit failure for invalid counts, budget
exhaustion, or allocation failure.

## Plan

- [x] Read repository instructions and the complete BR-0055 finding
- [x] Inspect related count and allocation findings, live archive ownership,
      all catalog consumers, cleanup paths, format budgets, and current tests
- [x] Replace the fixed file-entry array and truncation with checked dynamic
      allocation and complete parsing
- [x] Update consumers and cleanup without changing their iteration contract
- [x] Add exact-512, 513, larger-catalog, late-selected-entry, and allocation
      failure regressions
- [x] Run scoped code quality, focused Inno tests, all native extraction suites,
      and Android ABI builds
- [x] Finalize BR-0055 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality: PASS
- Focused `test_gog_fd`: PASS, including 512, 513, 1,024, and 4,096-entry
  catalogs, a selected final entry, allocation failure, the 4,097-entry
  rejection, and both real GOG installers
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
