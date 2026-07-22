# BR-0042 native JSON string plan

Date: 2026-07-21

## Goal

Resolve BR-0042 by encoding every untrusted native tool string as valid JSON without changing the tools' record schemas, while preserving existing handmade comments verbatim.

## Steps

- [x] Review the active finding, every named JSON producer and consumer, existing escaping helpers, and native test registration.
- [x] Add or reuse one byte-safe JSON string encoder with explicit malformed UTF-8 behavior.
- [x] Route every dynamic string field in the affected native tools through the shared encoder.
- [x] Add hostile quote, backslash, control-byte, newline, and malformed UTF-8 regression records parsed by a strict JSON consumer.
- [x] Run scoped code quality, focused JSON tests, and the complete native extraction suite.
- [x] If validation passes, move BR-0042 from the active ledger to the done ledger with resolution notes.

## Validation record

Scoped code quality passed for all changed native and CMake files. The focused
`json_writer_tests` strict-parser test passed, and `android/tests/test_cue_iso.ps1`
passed all 10 native suites on Windows. The regression directly covers hostile
bytes that Windows filenames cannot represent; a POSIX filesystem integration
run was not available in this tranche.
