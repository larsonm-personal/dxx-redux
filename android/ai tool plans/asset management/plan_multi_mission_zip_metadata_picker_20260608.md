# Multi-mission ZIP metadata picker

## Goal
When an imported ZIP contains multiple HOG plus mission descriptor sets, let the launcher choose which mission to analyze before showing level metadata.

## Plan
- [x] Inspect project instructions and the sample `descent_maximum_fixed.zip` contents.
- [x] Trace the current mission ZIP scan and metadata button flow.
- [x] Design a focused representation for multiple metadata targets without disrupting single-mission ZIP behavior.
- [x] Implement the picker and target construction changes.
- [x] Add focused tests for multiple HOG plus descriptor sets in one ZIP.
- [x] Run scoped code quality/tests and record the result.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths <touched files>` passed.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.MissionZipTest --tests com.dxxredux.app.LevelMetadataTargetsTest` passed.
