# BR-0060 validate complete ISO directory records

## Goal

Validate every ISO directory record as a complete bounded structure before
reading fixed fields, identifiers, padding, or system-use data.

## Plan

- [x] Read repository instructions and the complete BR-0060 finding
- [x] Perform the required P1 frozen-to-live disproof attempt and trace every
      directory-record parser, recursion path, and extraction consumer
- [x] Add one checked directory-record validator before all field access
- [x] Add short, overlong, wrapping, sector-padding, identifier, and exact-edge
      regressions with clean rejection and valid-media controls
- [x] Run scoped code quality, focused ISO tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0060 and move its complete finding and disposition entry to
      the done ledger

## Verification

- P1 frozen-to-live disproof: CONFIRMED, the live walker retained the frozen
  short-record field reads and invalid-name use, while the PVD root record and
  recursive error path added two more in-scope validation gaps
- Scoped code quality: PASS
- Focused ISO coverage: PASS for raw Mode 1 tracks and standalone images,
  including record lengths 1 through 33, exact and one-over identifiers,
  nonzero padding, a record crossing the final two sector bytes, a short PVD
  root record, and a record crossing the declared directory extent
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
- AddressSanitizer and MemorySanitizer: unavailable because neither the
  configured Windows host nor WSL has a runnable Clang or GCC toolchain
