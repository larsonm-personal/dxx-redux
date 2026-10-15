# SDK-free host JVM tests

## Goal
Run the existing metadata and proxy JVM test tasks without configuring the Android app, downloading AGP or requiring an SDK. Preserve the full Android build as the default. This does not move or duplicate production sources or test assertions.

## Plan
1. Bootstrap the pinned Gradle distribution and verify the installed Java 21 compiler
2. Add opt-in settings selection for existing pure JVM modules and a plugin-free root build
3. Run metadata and proxy test tasks and record actual test results, skips and environment blockers
4. Document commands and explicitly retain Android app/device coverage gaps

## Validation
- Gradle 9.8.0 with the repository-pinned Kotlin and library versions
- Default host mode: 10 metadata tests pass, 2 proxy tests pass, 1 historical comparison skipped by its existing opt-in
- Final offline rerun with `-PcompareProxyHistory=true --rerun-tasks --warning-mode all`: all 13 tests pass, zero skips/errors/failures
- All 8 actionable Gradle tasks execute; the host root shares Kotlin plugin classes and produces no duplicate-plugin warning
- Existing production sources, test sources and module builds match cmake base 46dad764386f81daff666dacaa8c3646458dfd2b
- Independent source review confirms default Android settings are retained
- No Android app unit tests or device checks have run. The app's AGP/SDK/dependencies remain required for its existing test task
- PowerShell formatting/catalog validation was not executed because the environment prohibits PowerShell. No top-level test entry or catalog owner changed
