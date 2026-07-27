# BR-0063 assemble ISO multi-extent files before extraction

## Goal

Represent each ISO file as one validated ordered extent chain and extract the
complete logical payload without exposing continuation records as separate
files or silently accepting malformed chains.

## Plan

- [x] Read repository instructions and the complete BR-0063 finding
- [x] Trace multi-extent flags, catalog publication, JNI listing, filtering,
      progress accounting, and raw and standalone extraction
- [x] Extend the ISO catalog model with bounded ordered extents and assemble
      continuation records with checked names, sizes, LBAs, and termination
- [x] Add valid two and three-extent, missing-final, mismatched-name,
      directory-flag, capacity, overflow, and ordinary-file regressions
- [x] Run scoped code quality, focused ISO tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0063 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality: PASS
- Focused ISO coverage: PASS for raw Mode 1 tracks and standalone images,
  including exact two and three-section concatenation; logical listing size
  and section order; mismatched, interrupted, missing-final, directory,
  misaligned, out-of-range, overlapping, and overflowing chains; exact 512
  and rejected 513-section pools; and existing ordinary single-extent files
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
- AddressSanitizer and UndefinedBehaviorSanitizer: unavailable because WSL has
  no runnable Clang or GCC toolchain
