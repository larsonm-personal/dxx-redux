# BR-0034 Inno file checksum plan

Date: 2026-07-21

## Goal

Resolve BR-0034 by verifying each supported Inno file checksum before output publication, while preserving existing handmade comments verbatim.

## Steps

- [ ] Review the ledger finding, version-specific checksum metadata, extraction paths, and existing Inno fixtures.
- [ ] Implement streaming checksum verification across stored and compressed extraction paths.
- [ ] Ensure output is committed only after verification and add corruption regression coverage.
- [ ] Run scoped code quality checks, focused Inno tests, and the complete native extraction suite.
- [ ] If validation passes, move BR-0034 from the active ledger to the done ledger with resolution notes.

## Validation record

Pending.
