# BR-0071 ARJ basic header validation

## Goal

Reject incomplete or physically inconsistent SOW ARJ entries before parsing
fixed fields, strings, extended headers, or compressed data, and preserve
malformed-input failures through extraction.

## Plan

- [x] Read repository instructions, both review ledgers, the complete finding,
      related findings, and the live parser and tests
- [x] Compare the frozen and live implementations and identify BR-0071 scope
      not already resolved by related remediation
- [x] Add complete basic-header, string, extended-span, position, and compressed
      span validation with explicit malformed-input propagation
- [x] Extend the registered SOW integrity suite with exact-boundary and malformed
      header fixtures
- [x] Run scoped code quality, focused SOW tests, the complete native extraction
      suite, and available sanitizer or Android ABI validation
- [x] Finalize BR-0071 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality: PASS
- Focused `sow_integrity_tests`: PASS
- `android/tests/test_cue_iso.ps1`: PASS, 13/13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
- MemorySanitizer, AddressSanitizer, and UndefinedBehaviorSanitizer: NOT RUN
  because the configured Windows host and WSL environment have no runnable
  Clang or GCC sanitizer toolchain
