# BR-0067 XAR TOC bounds

## Goal

Bound XAR compressed and uncompressed table-of-contents lengths before
allocation, reading, decompression, and XML parsing.

## Plan

- [x] Read repository instructions, review process, and the complete finding
- [x] Trace XAR header validation, TOC ownership, decompression, and cleanup
- [x] Implement checked format and policy limits with malformed coverage
- [x] Run scoped code quality, focused tests, native suites, and Android builds
- [x] Record the resolution and move BR-0067 to the done ledger

## Verification

- Scoped code quality passed
- All 17 extraction host tests passed, including `pkg_toc_bound_tests`
- All 24 D1 and 28 D2 native host tests passed
- Android debug builds passed for arm64-v8a, armeabi-v7a, and x86_64 with JDK 21
- The repository has no real PKG fixture or Android sanitizer runner, so those
  validation items were not available
