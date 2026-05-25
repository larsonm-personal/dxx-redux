# SetupActivity sections split - 2026-05-24

## Goal
Continue reducing `SetupActivity.kt` by moving reusable setup section composables and their small display helpers into `SetupSections.kt` without changing launcher state ownership.

## Plan
- [x] Confirm section composable dependencies and call sites.
- [x] Create `SetupSections.kt` for scroll arrows, section headers, mods, demos, music status, file detail/status rows, and display helpers.
- [x] Remove moved declarations from `SetupActivity.kt` and clean imports if safe.
- [x] Run focused diagnostics, JVM tests, and scoped code-quality checks.
- [x] Update this plan and the survey plan with results.

## Notes
- Leave `ControllerSection` and import dialogs in `SetupActivity.kt` for separate tranches.
- Keep helpers package-internal where the activity or later dialogs still call them. Use domain-specific names for package-visible helpers to avoid collisions with other pages.

## Results
- Added `SetupSections.kt` with `SetupScrollArrows`, setup section headers, mods/demos/music sections, mod detail UI, file detail dialog, file status/download rows, and missing-files help.
- Left `ControllerSection`, set management, and import dialogs in `SetupActivity.kt` for separate tranches.
- Kept the activity's remaining `formatSize` helper in place for import dialogs, and used private section-local formatting helpers in `SetupSections.kt` to avoid same-package name conflicts.
- Renamed the extracted scroll helper to `SetupScrollArrows` after compile found collisions with private `ScrollArrows` helpers in other pages.
- `SetupActivity.kt` is now 6635 lines; `SetupResumePanel.kt` is 263 lines and `SetupSections.kt` is 1284 lines after formatting.
- Validation passed with focused diagnostics, `:app:testDebugUnitTest`, and scoped `android\run-code-quality.ps1 -Fix -Paths ...`.
