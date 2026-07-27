# BR-0058 reject encrypted Inno chunks

## Goal

Preserve the Inno chunk-encryption flag and reject unsupported encrypted
entries before payload reads, allocation, decoding, or output publication.

## Plan

- [x] Read repository instructions and the complete BR-0058 finding
- [x] Trace data flags through parsing, analysis, JNI, CLI, and every extraction
      path, including related findings and existing capability documentation
- [x] Preserve encryption state and add one deterministic pre-payload rejection
- [x] Add stored and compressed encrypted-entry regressions with no output,
      plus unencrypted compatibility coverage
- [x] Run scoped code quality, focused Inno tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0058 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality: PASS
- Capability documentation consistency tests: PASS, all 4 tests
- Focused `test_gog_fd`: PASS, including parsed encryption metadata, stored,
  zlib, LZMA1, and LZMA2 early rejection without progress or output, an
  unencrypted stored control, and both real 5.6.2 and 5.5.7 Unicode installers
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
- AddressSanitizer and UndefinedBehaviorSanitizer: unavailable because neither
  the configured Windows host nor WSL has a runnable Clang or GCC toolchain
