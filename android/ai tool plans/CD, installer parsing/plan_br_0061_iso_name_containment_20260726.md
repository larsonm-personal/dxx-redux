# BR-0061 confine ISO names to the extraction root

## Goal

Reject ISO record names and derived paths that can escape or alias outside the
chosen extraction root on Android, Windows, or POSIX filesystems.

## Plan

- [x] Read repository instructions and the complete BR-0061 finding
- [x] Perform the required P1 frozen-to-live disproof attempt and trace record
      names through cleaning, path joining, listing, JNI, and extraction
- [x] Add one component and relative-path policy before catalog publication
      and retain a checked final output-root join
- [x] Add traversal, separator, absolute, drive, empty, dot, control, and
      exact-boundary regressions for raw tracks and standalone images
- [x] Run scoped code quality, focused ISO tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0061 and move its complete finding and disposition entry to
      the done ledger

## Verification

- P1 frozen-to-live disproof: CONFIRMED, the live name cleaner, relative join,
  output concatenation, and JNI delimiter protocol retained the frozen paths
- Scoped code quality: PASS
- Focused ISO coverage: PASS for raw Mode 1 tracks and standalone images,
  including slash, backslash, absolute, drive-like, dot, dot-dot, empty,
  control, high-byte, reserved-character, trailing-space, and pipe names;
  forged public-list traversal; exact 512 and rejected 513-entry catalogs; and
  valid nested name, size, payload, and extraction round-trips
- `:app:compileDebugKotlin`: PASS under JDK 21
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
- POSIX, AddressSanitizer, and UndefinedBehaviorSanitizer: unavailable because
  WSL has no runnable Clang or GCC toolchain
