# BR-0018 Extraction Budgets

## Goal

Apply explicit resource ceilings to every untrusted archive path named by
BR-0018, enforce them against declared and actual work, add boundary coverage,
and archive the finding only after the complete cross-language scope passes
validation

## Plan

- [ ] Define documented native and launcher limits for entries, expanded bytes,
  compression ratio, metadata, and captured subprocess output
- [ ] Enforce native limits in StuffIt/STi2, HFS, SOW, Inno, PKG, and ISO paths
- [ ] Enforce launcher limits in archive opening, mission inspection, mission
  music inspection/staging, and durable extraction
- [ ] Bound PowerShell ZIP/7z/native-child inspection and DOS demo staging
- [ ] Add focused boundary tests for native, Kotlin, and PowerShell paths
- [ ] Run scoped code quality and relevant native, Gradle, and PowerShell tests
- [ ] Move BR-0018 to the done ledger only if every named scope is resolved

