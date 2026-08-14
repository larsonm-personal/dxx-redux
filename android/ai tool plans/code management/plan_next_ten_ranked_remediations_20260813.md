# Next ten ranked remediations plan - 2026-08-13

## Objective

Complete impact ranks 23 through 32 from the active general code-quality
ledger, using one fresh Sol-medium worker per remediation and keeping original
D1/D2 changes at zero wherever possible

## Queue

- [x] GQR-0091 / GQF-0104 - Kotlin bounded-read peak-live-memory policy
- [x] GQR-0095 / GQF-0108 - no weak fallback after RAR policy rejection
- [x] GQR-0096 / GQF-0109 - bounded RAR enumeration before materialization
- [x] GQR-0105 / GQF-0118 - one catalog budget through nested music containers
- [x] GQR-0106 / GQF-0119 - nested streaming size and expansion accounting
- [x] GQR-0112 / GQF-0125 - admitted Python runtime for bounded extraction
- [x] GQR-0114 / GQF-0127 - supervised bounded-extractor process trees
- [x] GQR-0115 / GQF-0128 - reject unsafe extractor output types
- [x] GQR-0005 / GQF-0001 through GQF-0004 - remove tracked runtime artifacts
- [x] GQR-0039 / GQF-0052 - exception-safe assigned JNI acquisitions

## Dependency waves

- [x] Wave 1: GQR-0091, GQR-0095, GQR-0105
- [x] Wave 2: GQR-0096 after GQR-0095, GQR-0106 after GQR-0105, GQR-0112
- [x] Wave 3: GQR-0114 after GQR-0112, GQR-0005, GQR-0039
- [x] Wave 4: GQR-0115 after GQR-0114

## Acceptance

- [x] Freeze ranked queue and starting worktree
- [x] Independently review each item diff and terminal evidence
- [x] Run combined scoped quality, focused tests, Windows builds, and Android ABIs
- [x] Update the canonical ledger as one writer and audit all terminal states

## Completion

- Completed: 2026-08-13
- All ten ranked remediations are `DONE`
- Thirteen associated findings are `FIXED`
- Combined Windows D1/D2 builds and Android native builds for all three ABIs passed
- Combined focused Kotlin tests passed
- Item plans record the Python, PowerShell, Windows, WSL, real-package, JNI, artifact-policy, quality and diff checks
- Inherited D1/D2 source impact from this batch is zero

## Starting state

- HEAD: `87925fa4162a672842fa3a81015f302bb72d6143`
- Existing dirty tree includes the completed prior ten-item batch and unrelated
  route-metadata background-precompute work
- Workers must preserve all concurrent work and must not edit the canonical
  ledger or this campaign plan
