# Descriptor and HOG metadata pairing

## Goal
Allow level metadata to open from either file in a mission descriptor plus HOG pair.

## Plan
- [x] Re-read project instructions and inspect current `.mn2` metadata target paths.
- [x] Identify why descriptor-only metadata misses its matching HOG.
- [x] Link direct and ZIP-constituent descriptors to same-stem HOG assets for analysis.
- [x] Add focused tests for `.mn2` plus `.hog` pairing.
- [x] Run scoped code quality/tests and record the result.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths <touched files>` passed.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.LevelMetadataTargetsTest` passed.
