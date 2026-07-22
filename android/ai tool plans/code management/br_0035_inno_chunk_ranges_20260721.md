# BR-0035 Inno chunk range plan

Date: 2026-07-21

## Goal

Resolve BR-0035 by proving every Inno file range is contained within its declared chunk before allocation, decoding, pointer arithmetic, or output publication, while preserving existing handmade comments verbatim.

## Steps

- [x] Review the active finding, buffered and streaming range arithmetic, archive span metadata, and current Inno tests.
- [x] Centralize overflow-safe file and physical chunk range validation for stored and compressed entries.
- [x] Apply the validator before allocation, decoding, reads, and pointer formation in every extraction path.
- [x] Add boundary, one-byte-over, adjacent-sentinel, and near-`UINT64_MAX` regression coverage with no committed output.
- [x] Run scoped code quality checks and the complete native extraction suite.
- [x] If validation passes, move BR-0035 from the active ledger to the done ledger with resolution notes.

## Validation record

- Scoped code quality passed for the Inno reader, public header, GOG regression test, plan, and ledgers.
- The native extraction build completed without new compiler warnings.
- All 9 native extraction suites passed, including the real Inno 5.5.7 and 5.6.2 fixtures.
- Stored and zlib regression cases cover exact boundaries, adjacent sentinels, physical truncation, streaming dispatch, and wrapping offsets with no final output on failure.
- BR-0035 was removed from the active ledger and archived as fixed in the done ledger.
- AddressSanitizer and UndefinedBehaviorSanitizer were unavailable in the Windows test toolchain. Independent P1 verification remains a campaign closure action.
