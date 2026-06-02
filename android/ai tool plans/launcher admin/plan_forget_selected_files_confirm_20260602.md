# Plan: Confirm Forget Selected Game Files

## Goal
Add an OK/Cancel confirmation dialog before forgetting individually selected game files, with OK as the default controller action.

## Steps
- [completed] Locate the launcher forget-selected-files action and existing dialog/controller-focus conventions.
- [completed] Add a confirmation state and dialog around the selected-file forget action.
- [completed] Verify with targeted build or tests, and update this plan with results.

## Results
- Added a confirmation dialog before the per-file `Forget` action in `FileDetailDialog`.
- The dialog uses `OK` and `Cancel`; `OK` receives launcher controller focus when the dialog opens.
- Verification passed:
  - `android\run-code-quality.ps1 --fix`
  - `.\gradlew.bat :app:compileDebugKotlin`
