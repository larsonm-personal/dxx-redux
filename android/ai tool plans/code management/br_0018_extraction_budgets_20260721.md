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
- [x] Enforce launcher limits in archive opening, mission inspection, mission
  music inspection/staging, and durable extraction
- [x] Bound PowerShell ZIP/7z/native-child inspection, DOS demo staging, and
  mission-music fingerprint extraction
- [x] Add focused native boundary tests
- [x] Run scoped native code quality and the nine-test extraction suite
- [x] Add Kotlin and PowerShell boundary tests and run their relevant checks
- [x] Move BR-0018 to the done ledger only if every named scope is resolved

## Legacy Mac CD tranche

- [x] Bound raw-track, HFS-image, per-file, aggregate-file, and catalog-entry work
- [x] Run unar with wall-clock, diagnostic, entry, per-file, aggregate-output, and ratio ceilings
- [x] Add focused helper boundary tests and retain real-media native extraction coverage
- [x] Update BR-0018's partial disposition without archiving the still-open finding

## Current disposition

Completed on 2026-07-21 after account verification allowed the restricted
scope to resume. Native decoders, Kotlin launcher archive and mission paths,
legacy Mac CD extraction, and the remaining PowerShell batch paths now enforce
explicit resource ceilings. Focused Kotlin, Python, and PowerShell tests,
scoped code quality, AST parsing, and all nine native extraction suites pass.
BR-0018 has been moved to the done ledger
