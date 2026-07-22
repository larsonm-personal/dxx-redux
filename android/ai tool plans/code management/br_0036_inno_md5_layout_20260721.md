# BR-0036 Inno MD5 layout plan

Date: 2026-07-21

## Goal

Resolve BR-0036 by consuming and enforcing the pre-5.3.9 Inno MD5 fields through one version-layout decision, while preserving existing handmade comments verbatim.

## Steps

- [x] Review the finding, main-header and data-entry parsers, checksum representation, extraction verification, and available test seams.
- [x] Centralize the 5.3.9 MD5-to-SHA-1 layout transition and consume the correct password and file digest widths.
- [x] Add streaming MD5 verification to buffered, streamed, and Galaxy extraction paths.
- [x] Add multi-entry 5.3.8 and 5.3.9 transition fixtures plus valid and corrupt MD5 extraction coverage.
- [x] Run scoped code quality checks and the complete native extraction suite.
- [x] If validation passes, move BR-0036 from the active ledger to the done ledger with resolution notes.

## Validation record

- Scoped code quality passed for the Inno reader, public header, native GOG test, CMake registration, capability matrix, documentation test, plan, and ledgers.
- All 4 Inno capability documentation tests passed.
- The native extraction build completed without new project warnings.
- All 9 native extraction suites passed, including real Inno 5.5.7 and 5.6.2 fixtures.
- Faithful 5.3.0, 5.3.8, and 5.3.9 fixtures verify password and file-digest widths, subsequent compression and flag alignment, multi-entry parsing, valid MD5 extraction, and mismatch rejection without output.
- BR-0036 was removed from the active ledger and archived as fixed in the done ledger.
