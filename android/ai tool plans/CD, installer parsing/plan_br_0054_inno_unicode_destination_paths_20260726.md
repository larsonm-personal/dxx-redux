# BR-0054 preserve distinct Unicode Inno destination paths

## Goal

Decode, store, compare, publish, and extract supported Inno destination paths
without collapsing distinct Unicode names or creating lossy path collisions.

## Plan

- [x] Read repository instructions and the complete BR-0054 finding
- [x] Compare the frozen and live implementations and trace destination paths
      through decoding, archive storage, filtering, JNI, CLI, and extraction
- [x] Define one bounded Unicode path representation and conversion policy that
      preserves distinct supported names and fails closed on malformed input
- [x] Add non-ASCII, supplementary, malformed, truncation, and collision
      regressions while retaining real-installer compatibility
- [x] Run scoped code quality, focused Inno tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0054 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Frozen-to-live trace: CONFIRMED, the live decoder retained the frozen lossy
  ASCII substitution, embedded-NUL deletion, odd-byte acceptance, and silent
  destination truncation
- Focused `test_gog_fd`: PASS against synthetic Unicode boundary and collision
  cases plus the real Inno 5.6.2 and 5.5.7 installers
- Scoped code quality: PASS for all changed C, header, JNI, CLI, test, and plan
  files
- Native extraction suite: PASS, 13 of 13 registered tests
- Android native build: PASS for arm64-v8a, armeabi-v7a, and x86_64
- Sanitizers: unavailable because neither the Windows PATH nor WSL contains a
  Clang or GCC toolchain
