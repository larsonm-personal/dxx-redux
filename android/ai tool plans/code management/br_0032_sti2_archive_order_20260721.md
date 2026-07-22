# BR-0032 STi2 archive-order enforcement plan

Date: 2026-07-21

## Goal

Resolve BR-0032 by parsing STi2 entries in archive order and rejecting inputs that do not match the declared structure, while preserving existing handmade comments verbatim.

## Steps

- [x] Review the ledger finding, STi2 format assumptions, current parser, and existing tests.
- [x] Implement archive-order parsing and declared-structure validation with bounded, focused changes.
- [x] Add regression coverage for valid archives and malformed ordering or structure.
- [x] Run scoped code quality checks and the relevant native test suite.
- [x] If validation passes, move BR-0032 from the active ledger to the done ledger with resolution notes.

## Validation record

- Scoped code quality passed for `sti2_extract.c` and `test_sti2.c`.
- `android/tests/test_cue_iso.ps1` passed all 9 native extraction suites.
- Existing handmade comment lines in the touched C files were preserved.
