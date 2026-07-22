# BR-0018 Extraction Budgets

## Goal

Apply explicit resource ceilings to every untrusted archive path named by
BR-0018, enforce them against declared and actual work, add boundary coverage,
and archive the finding only after the complete cross-language scope passes
validation

## Plan

- [x] Define and document native limits for entries, expanded bytes,
  compression ratio, metadata, and extraction memory
- [x] Enforce native limits in StuffIt/STi2, HFS, SOW, Inno, PKG, and ISO paths
- [ ] Enforce launcher limits in archive opening, mission inspection, mission
  music inspection/staging, and durable extraction
- [ ] Bound PowerShell ZIP/7z/native-child inspection and DOS demo staging
- [x] Add focused native boundary tests
- [x] Run scoped native code quality and the nine-test extraction suite
- [ ] Add Kotlin and PowerShell boundary tests and run their relevant checks
- [ ] Move BR-0018 to the done ledger only if every named scope is resolved

## Current disposition

Partially finished on 2026-07-21. The native reader and decoder scope is
implemented and validated. The Kotlin launcher and PowerShell portions remain
open because the GPT cybersecurity restrictions prevented completing that
scope. BR-0018 therefore remains in the active ledger with the residual risk
recorded instead of being archived as a completed finding
