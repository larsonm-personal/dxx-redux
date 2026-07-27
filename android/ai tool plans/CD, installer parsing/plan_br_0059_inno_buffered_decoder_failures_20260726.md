# BR-0059 propagate buffered Inno decoder failures

## Goal

Require every buffered Inno decoder to report complete, valid stream
termination and propagate malformed, truncated, and output-bound failures
without publishing partial data.

## Plan

- [x] Read repository instructions and the complete BR-0059 finding
- [x] Trace stored, zlib, LZMA1, LZMA2, and BZip2 method selection through
      buffered and streaming paths, including decoder status and cleanup
- [x] Make buffered decoder success depend on valid terminal status and exact
      input/output invariants, propagating every failure to extraction
- [x] Add malformed, truncated, trailing-data, and output-bound regressions
      with no output publication, plus valid compatibility controls
- [x] Run scoped code quality, focused Inno tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0059 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality: PASS
- Focused `test_gog_fd`: PASS, including valid zlib, LZMA1, and LZMA2 streams;
  truncated nonsolid and later solid ranges; malformed headers and properties;
  trailing input; zlib dictionary requests; LZMA no-progress inputs; and both
  real 5.6.2 and 5.5.7 Unicode installers
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
- AddressSanitizer and UndefinedBehaviorSanitizer: unavailable because neither
  the configured Windows host nor WSL has a runnable Clang or GCC toolchain
