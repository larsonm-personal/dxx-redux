# Vertigo mission import recognition

## Goal
Allow Android setup file import to accept Vertigo mission descriptor files alongside their HOGs.

## Plan
- [x] Inspect the Vertigo extracted mission files and launcher import recognition path.
- [x] Fix the Android import classifier so `.mn2` mission descriptors are treated as importable game data.
- [x] Add or update focused tests for direct file import recognition.
- [x] Run scoped code quality/tests and record the result.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths <touched files>` passed.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ImportTreeScannerTest` passed.
