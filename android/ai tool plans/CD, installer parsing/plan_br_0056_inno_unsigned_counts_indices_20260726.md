# BR-0056 unsigned Inno counts and indices

## Goal

Keep Inno entry counts and file locations in compatible unsigned or `size_t`
types from parsing through allocation, listing, and extraction, rejecting every
invalid value before allocation or indexing.

## Plan

- [x] Read repository instructions and the complete BR-0056 finding
- [x] Perform the required P1 frozen-to-live disproof attempt and inspect all
      count, location, allocation, listing, extraction, and cleanup paths
- [x] Standardize validated counts and indices without signed narrowing
- [x] Enforce entry-budget, decompressed-capacity, allocation-size, sentinel,
      and backing-pointer checks before every array access
- [x] Add zero, budget maximum, `INT_MAX`, `INT_MAX + 1`,
      `UINT32_MAX - 1`, `UINT32_MAX`, location, and allocation-failure tests
- [x] Run scoped code quality, focused Inno tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0056 and move its complete finding and disposition entry to
      the done ledger

## Verification

- P1 frozen-to-live disproof: the live 4,096-entry budget already rejected the
  oversized-count null-array trigger before its signed cast, but the high-bit
  file-location cast remained exploitable and was confirmed
- Scoped code quality: PASS
- Focused `test_gog_fd`: PASS, including counts and locations at zero, 4,096,
  `INT_MAX`, `INT_MAX + 1`, `UINT32_MAX - 1`, and `UINT32_MAX`, backing-array
  absence, allocation failure, and both real 5.6.2 and 5.5.7 Unicode installers
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS without new compiler warnings for
  arm64-v8a, armeabi-v7a, and x86_64
- AddressSanitizer and UndefinedBehaviorSanitizer: unavailable because neither
  the configured Windows host nor WSL has a runnable Clang or GCC toolchain
