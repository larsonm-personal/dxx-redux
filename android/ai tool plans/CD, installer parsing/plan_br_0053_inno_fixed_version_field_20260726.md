# BR-0053 bounded Inno version identifier parsing

## Goal

Parse the 64-byte Inno setup version identifier only within its fixed field,
reject malformed or unsupported identifiers deterministically, and preserve
all supported real version layouts.

## Plan

- [x] Read repository instructions, the review process, and the complete
      BR-0053 finding
- [x] Confirm the required independent P1 verification and inspect related
      findings, the frozen implementation, live parser, format notes, and tests
- [x] Replace unbounded C-string parsing with bounded terminator, digit,
      overflow, separator, suffix, and complete-grammar validation
- [x] Add fixed-field regressions for termination boundaries, malformed and
      overflowing components, suffixes, trailing bytes, and supported IDs
- [x] Run scoped code quality, focused Inno tests, all native extraction suites,
      sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0053 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Pre-remediation frozen-to-live trace: CONFIRMED, the 64-byte stack field had
  no terminator and reached unbounded `strtol` and `strstr` calls
- Scoped code quality: PASS
- Focused `test_gog_fd`: PASS, including both real 5.6.2 and 5.5.7 Unicode
  installers
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
- AddressSanitizer and UndefinedBehaviorSanitizer: unavailable because neither
  the configured Windows host nor WSL has a runnable Clang or GCC toolchain
