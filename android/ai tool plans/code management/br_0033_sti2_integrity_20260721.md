# BR-0033 STi2 payload integrity plan

Date: 2026-07-21

## Goal

Resolve BR-0033 by verifying STi2 fork checksums, rejecting compressed-input exhaustion, and preventing failed extraction from leaving committed output, while preserving existing handmade comments verbatim.

## Steps

- [x] Review the ledger finding, STi2 checksum metadata, decoder readers, extraction commit path, and existing fixtures.
- [x] Preserve fork checksum metadata and enforce complete decoder input and output integrity.
- [x] Make output publication transactional and add corruption and truncation regression coverage.
- [x] Run scoped code quality checks and the complete native extraction suite.
- [x] If validation passes, move BR-0033 from the active ledger to the done ledger with resolution notes.

## Validation record

- Scoped code quality passed for the four touched STi2 source and test files.
- `android/tests/test_cue_iso.ps1` passed all 9 native extraction suites.
- Stored corruption and checksum mismatches fail without output; real method 13 and 15 corpus streams reject material truncation and method 15 trailer corruption.
- Method 14 uses the same tested hard-exhaustion reader as method 13 and retains its focused tree coverage; no real method 14 archive fixture is registered.
- AddressSanitizer and UndefinedBehaviorSanitizer execution was unavailable in the Windows test toolchain.
- Existing handmade comment lines in the touched source and test files were preserved.
