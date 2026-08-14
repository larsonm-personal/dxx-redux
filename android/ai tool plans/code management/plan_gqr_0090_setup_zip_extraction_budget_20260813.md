# GQR-0090 Setup ZIP extraction budget plan

## Scope

Close GQF-0103 by carrying one attempt-owned `ExtractionBudget` through direct Setup ZIP extraction, including selected outer entries and nested SOW output, without weakening the generic 16 MiB ZIP preamble admission

## Plan

- [x] Map direct Setup ZIP callers, selected-entry copies, nested SOW extraction, and existing budget primitives
- [x] Add attempt-owned budget plumbing and bounded selected-entry copies
- [x] Admit nested SOW outputs into the same entry and expanded-byte budget
- [x] Add focused exact-limit and one-over tests for entry count, per-entry bytes, aggregate bytes, and nested/direct composition
- [x] Run scoped formatting and focused JVM/Android validation
- [x] Record final paths, checks, and remaining boundaries here

## Constraints

- Do not edit the canonical quality ledger or parent campaign plan
- Preserve GQR-0087's 16 MiB generic stream admission
- Reuse `ExtractionBudget` rather than adding a second extraction policy
- Keep all changes in branch-original Android code and tests

## Results

- `SetupActivity.kt` creates one budget for the selected direct-archive attempt and passes it to each ZIP extraction
- `SetupFileImport.kt` registers every traversed ZIP member, accounts every selected output chunk, rechecks completed-entry ratio metadata, and removes direct and nested staging on failure or cancellation
- Nested SOW extraction starts from the Kotlin budget's cumulative byte and entry counters; successful native counters are accepted back into the same authoritative budget
- `ExtractionLimitsTest.kt` covers exact and one-over per-entry, entry-count, direct aggregate, nested aggregate, cancellation-state, and staging-cleanup boundaries
- The 16 MiB generic ZIP stream admission in `ArchiveInputStreams.kt` is unchanged

## Validation

- `android/run-code-quality.ps1 -Fix` on the touched Kotlin and plan paths: passed
- `gradlew.bat testDebugUnitTest --tests com.dxxredux.app.ExtractionLimitsTest`: passed, 32 tasks, 59 seconds
- `gradlew.bat assembleDebug`: exceeded the 120-second command window while native compilation was still active; no diagnostic was produced before timeout
- `git diff --check`: passed
