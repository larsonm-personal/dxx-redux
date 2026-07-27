# BR-0062 make ISO directory traversal cycle-safe and fail closed

## Goal

Make ISO directory traversal terminate deterministically and reject cyclic,
repeated, unreadable, over-capacity, or arithmetically invalid directory trees
without publishing a partial catalog.

## Plan

- [x] Read repository instructions and the complete BR-0062 finding
- [x] Perform the required P1 frozen-to-live disproof attempt and trace all
      directory recursion, extent accounting, capacity, and error propagation
- [x] Add bounded active and visited directory tracking with checked extent
      arithmetic and fail-closed traversal semantics
- [x] Add self-cycle, ancestor-cycle, duplicate, fan-out, depth, unreadable
      child, capacity, overflow, and valid nested regressions
- [x] Run scoped code quality, focused ISO tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0062 and move its complete finding and disposition entry to
      the done ledger

## Verification

- P1 frozen-to-live disproof: CONFIRMED for cycles and unchecked directory LBA
  spans; recursive propagation, overflow-safe rounding, and fail-closed
  capacity were already present from the preceding ISO remediations
- Scoped code quality: PASS
- Focused ISO coverage: PASS for raw Mode 1 tracks and standalone images,
  including self, ancestor, branching/repeated, and partially overlapping
  extents; unreadable children; `UINT_MAX` size and wrapped LBA spans; the
  4,097th traversal sector; exact depth 16 and rejected depth 17; exact 512 and
  rejected 513-entry catalogs; empty results on every failure; and valid
  nested traversal and extraction
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
- AddressSanitizer and UndefinedBehaviorSanitizer: unavailable because WSL has
  no runnable Clang or GCC toolchain
