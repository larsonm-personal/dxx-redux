# Next ten ranked remediations plan - 2026-08-12

## Objective

Complete the next ten live impact-ranked remediation items after GQR-0034,
using one fresh Sol-medium worker per item and preserving zero or minimal impact
on original D1/D2 files.

## Queue

- [x] GQR-0038 / GQF-0051 - STi2 method-15 peak-live-memory budget
- [x] GQR-0042 / GQF-0055 - reconnect route proof before state mutation
- [x] GQR-0043 / GQF-0056 - versioned domain-separated reconnect transcript
- [x] GQR-0044 / GQF-0057 - bounded unauthenticated reconnect verification
- [x] GQR-0048 / GQF-0061 - one budget through complete CD composition
- [x] GQR-0063 / GQF-0076 - Inno metadata peak-live-memory budget
- [x] GQR-0065 / GQF-0078 - overflow-free Inno version admission
- [x] GQR-0070 / GQF-0083 - aggregate Inno solid-chunk decode-work budget
- [x] GQR-0087 / GQF-0100 - validate ZIP structure before prompt-limit lift
- [x] GQR-0090 / GQF-0103 - extraction budget for direct Setup ZIP import

## Process

- [x] Freeze ranked queue and starting worktree
- [x] Run implementation workers in non-overlapping waves
- [x] Independently review each worker diff and required acceptance evidence
- [x] Run combined scoped quality, host, Android, and focused regression gates
- [x] Update the canonical ledger as one writer and audit all ten terminal states

## Completion

- Completed: 2026-08-13
- All ten ranked remediations are `DONE` and their findings are `FIXED`
- Combined Windows D1/D2 builds passed
- Android native D1/D2 builds passed for arm64-v8a, armeabi-v7a, and x86_64
- Combined focused ZIP, extraction-limit, CD, reconnect, STi2, and Inno validation passed as recorded in the canonical ledger and item plans

## Starting state

- HEAD: `322665c9e86b6141c0faa5e0ef796944f82ab263`
- Existing dirty path: unrelated route metadata performance implementation plan
- Canonical ledger is reserved for the root writer to prevent worker conflicts
