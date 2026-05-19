# DXA download suffix import plan

## Goal
- Allow launcher mod import to recognize DXA files with Android/browser duplicate suffixes such as `.dxa (1)`
- Keep unsupported file formats rejected cleanly
- Add focused test coverage for suffixed DXA filenames

## Tasks
- [x] Inspect current mod import filename and archive detection logic
- [x] Add normalized DXA filename handling for trailing text after `.dxa`
- [x] Add or update focused JVM tests
- [x] Run focused validation and formatting checks

## Validation
- `:app:testDebugUnitTest --tests com.dxxredux.app.LauncherFileLabelsTest --tests com.dxxredux.app.ImportTreeScannerTest --tests com.dxxredux.app.ModManagerDetailsTest`
- `android\run-code-quality.ps1 -Fix -Paths ...` plus direct ktlint on touched Kotlin files
