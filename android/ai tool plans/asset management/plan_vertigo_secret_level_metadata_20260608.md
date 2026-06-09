# Vertigo secret level metadata

## Goal
Show Vertigo mission secret levels as secret levels in Android level metadata instead of levels 21-23.

## Plan
- [x] Inspect `D2X.MN2` and confirm its normal/secret level declaration.
- [x] Trace Android level metadata target construction for mission descriptors and HOGs.
- [x] Fix secret-level file propagation for Vertigo-style descriptors.
- [x] Add focused tests that preserve Vertigo secret levels as secret-level analyzer inputs.
- [x] Run scoped code quality/tests and record the result.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths <touched files>` passed.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.LevelMetadataTargetsTest` passed.
