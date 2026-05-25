# ControllerConfigPage model/store split - 2026-05-24

## Goal
Reduce `ControllerConfigPage.kt` by moving controller binding model constants and config serialization helpers into focused same-package files while leaving the Compose page, picker dialogs, and preview drawing in place.

## Plan
- [x] Confirm data/store helper boundaries and current test coverage.
- [x] Move physical control maps, kconfig index maps, function lists, thresholds, exponent helpers, and shared assignment policy to `ControllerConfigModel.kt`.
- [x] Move joy setting builders, controller config load/save, mixer map creation, and loaded config model to `ControllerConfigStore.kt`.
- [x] Keep UI modifiers, picker dialogs, preview canvas drawing, and page state orchestration in `ControllerConfigPage.kt`.
- [x] Run focused diagnostics, controller JVM tests, scoped code-quality checks, ASCII checks on new files, and line-count checks.
- [x] Update this plan and the survey plan with results.

## Notes
- Keep names stable because existing tests reference many helpers directly.
- Prefer `internal` for helpers that were file-private but are still used by `ControllerConfigPage.kt` after the split.
- Use before/after line counts because new untracked files make raw diff stats look like pure additions.

## Results
- Added `ControllerConfigModel.kt` with physical control maps, kconfig index maps, function lists, default binding loading, threshold/exponent helpers, axis-cover policy, half-axis options, and shared assignment helpers.
- Added `ControllerConfigStore.kt` with joy setting builders, controller config load/save, mixer button map construction, and `LoadedConfig`.
- Kept UI modifiers, picker state, picker dialogs, preview drawing, and page orchestration in `ControllerConfigPage.kt`. `StickPickerResult` stayed with the picker UI.
- `ControllerConfigPage.kt` was 3284 lines before this tranche and is 2565 lines after formatting. The two new files total 702 lines, for about 17 net fewer lines.
- Validation passed with focused diagnostics, ASCII scan on new files, `:app:testDebugUnitTest` before and after formatting, and scoped `android\run-code-quality.ps1 -Fix -Paths ...`.
