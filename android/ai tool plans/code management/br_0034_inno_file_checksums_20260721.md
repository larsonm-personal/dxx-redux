# BR-0034 Inno file checksum plan

Date: 2026-07-21

## Goal

Resolve BR-0034 by verifying each supported Inno file checksum before output publication, while preserving existing handmade comments verbatim.

## Steps

- [x] Review the ledger finding, version-specific checksum metadata, extraction paths, and existing Inno fixtures.
- [x] Implement streaming checksum verification across stored and compressed extraction paths.
- [x] Ensure output is committed only after verification and add corruption regression coverage.
- [x] Run scoped code quality checks, focused Inno tests, and the complete native extraction suite.
- [x] If validation passes, move BR-0034 from the active ledger to the done ledger with resolution notes.

## Validation record

- Scoped code quality passed for the Inno reader, public header, GOG regression test, plan, and ledgers.
- The native extraction build completed without new compiler warnings.
- All 9 native extraction suites passed, including real Inno 5.5.7 and 5.6.2 checksum decisions.
- Synthetic Galaxy coverage verified checksum-before-filter ordering, valid output, mismatch rejection, and no final output on failure.
- BR-0034 was removed from the active ledger and archived as fixed in the done ledger. BR-0036 remains open for pre-5.3.9 MD5 layout compatibility.
