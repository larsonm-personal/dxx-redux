# Harden Gradle unit test state

## Plan

- [x] Inspect the failed suite report and identify the exact Gradle unit-test failure
- [x] Trace the fixture and archive-budget behavior for stale or cross-test state
- [x] Harden the test fixture or production boundary without changing timeouts
- [x] Run scoped formatting and the complete Gradle unit-test suite from a contaminated-state scenario
- [x] Review the final diff and record verification results

## Findings

- `missionZipWithLargeNestedHogImportsDurableExtraction` generated a 33 MiB all-zero entry with default compression
- The fixture exceeded the production 1000:1 expansion-ratio guard, so the security check correctly rejected it before the intended durable-extraction assertion
- Best-speed compression keeps the fixture under the outer archive threshold while leaving its ratio comfortably valid
- The new source-size assertion prevents the test from silently switching to the unrelated outer-file-size durability branch

## Verification

- Direct ktlint check passed for `ModManagerMissionZipTest.kt`; one existing extra blank line in that file was auto-formatted
- The complete 492-test Gradle unit suite passed after seeding its fixed working directory with a corrupt archive and stale extracted output; 1 test was skipped
- The stale sentinel was removed by test setup, and a forced second complete suite passed without cleaning; 492 passed with 1 skipped
- Windows CMake configure/build completed successfully for D1 and D2
- `git diff --check` passed
