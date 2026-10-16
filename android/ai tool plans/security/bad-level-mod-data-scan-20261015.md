# Bad level and mod data security scan

## Goal

Audit hostile mission, level, archive, and mod-controlled data paths for crashes or memory-unsafe behavior. Start with the repository-wide attack surface, narrow to the highest-risk modules, then inspect every approximately 500-line block in those modules through the final block. Keep an evidence-backed fix ledger and propose remediations.

## Method

1. Inventory all externally controlled formats and ingestion boundaries
2. Rank modules by direct parsing, allocation, indexing, path handling, and downstream trust
3. Inspect selected modules in contiguous approximately 500-line blocks, including D1/D2 counterparts
4. Corroborate candidates with call sites, declarations, tests, compiler diagnostics, and sanitizers where practical
5. Record findings, non-findings, coverage, and concrete fixes in `android/security_scan_bad_level_mod_data_20261015.md`

## Progress

- [x] Load repository instructions and inspect worktree state
- [x] Complete high-level attack-surface inventory
- [x] Select and document deep-scan module scope
- [x] Inspect every selected approximately 500-line block through the final block
- [x] Validate high-confidence findings against declarations, call sites, and fatal-error behavior
- [x] Run focused existing archive/mod safety tests
- [x] Complete prioritized fix ledger and remediation proposals

## Result

Completed. The report records 17 fix areas: 7 critical, 7 high, and 3 medium. No runtime fixes were made as part of this scan.
