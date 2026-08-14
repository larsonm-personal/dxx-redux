# GQR-0005 runtime artifact hygiene remediation

## Goal

Close `GQR-0005` / `GQF-0001` through `GQF-0004` by removing the exact
tracked runtime and scratch artifacts after proving they are not maintained
fixtures, then prevent their recurrence without hiding legitimate evidence

## Plan

- [x] Confirm the exact target paths are present, tracked, and match the
  canonical finding inventory
- [x] Audit Git provenance, repository consumers, generators, and fixture
  dependencies for every target
- [x] Remove only the audited target paths and add path-specific ignore rules
- [x] Add a focused tracked-artifact policy regression with an explicit,
  narrow inventory
- [x] Run the policy test, scoped code quality, diff checks, and final status
  and size audits
- [x] Record every removal, recoverability, validation result, and blocker

## Constraints

- Do not remove maintained fixtures, source assets, or review evidence
- Do not use broad database, transcript, debug-log, or text-file ignore rules
- Preserve concurrent work and do not edit the canonical ledger or campaign
  plan
- Keep all policy changes in branch-added repository-management paths

## Result

- Removed the root matchmaking database, WAL, and shared-memory runtime state
- Removed the dated emulator-copy debug log and Android build and test
  transcripts
- Removed the root `.tmp`, `test.txt`, and `d1_d2_ogl_diff.txt` scratch files
- The nine deletions remove 19,150,156 bytes, including 57,330 lines from
  text artifacts. Every deleted byte remains recoverable from Git history
- Root-anchored ignore rules cover only those runtime locations and recurrence
  shapes. They do not introduce repository-wide database, debug-log, output,
  scratch, or text-file exclusions
- The repository policy regression rejects tracked recurrence, verifies each
  removed path is ignored, and verifies the maintained synthetic Redbook disc
  remains tracked and visible

## Audit evidence

- Git provenance shows the database trio came from
  `4a5b331bf15313f83a96e56783a8ecbef28dfd62`, the output transcripts from
  `6218b4a313cbb422017d11f0f5a82cc3be5b5e70` and
  `3d9eebebd3c99faf25985ea322aa5a9941c7602b`, the root scratch files from
  `ec705c3f5f97764a21d29de2cd31acd04abdfbb7`, and the debug log from
  `7e6599a4630834ce007626fb478f45f6c8f7d316`
- Repository searches found no exact consumer of any target. The database
  name is a server runtime default, while the removed copy was at repository
  root rather than under the server working directory
- The empty `.tmp`, failed-command `test.txt`, and stale D1/D2 OGL comparison
  are neither source inputs nor maintained evidence
- The maintained `android/tests/test_redbook_data/test_disc.bin` fixture has
  direct CUE, PowerShell, and game-script consumers and was intentionally
  retained

## Validation

- `android/tests/test_repository_artifact_policy.ps1`: passed
- Scoped code quality for `.gitignore`, the policy test, and this plan: passed
- Every removed location resolves to its intended root-anchored ignore rule
- `android/tests/test_redbook_data/test_disc.bin` remains tracked and is not
  ignored
- Scoped and repository-wide `git diff --check`: passed
- No inherited D1 or D2 source file was changed

There are no implementation blockers
