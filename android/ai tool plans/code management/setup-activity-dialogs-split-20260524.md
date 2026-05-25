# SetupActivity dialogs split - 2026-05-24

## Goal
Continue reducing `SetupActivity.kt` by moving set management and import dialog composables into `SetupDialogs.kt` while keeping total line count roughly stable.

## Plan
- [x] Confirm dialog block boundaries, call sites, and helper dependencies.
- [x] Move `SetManagementDialog`, GOG/SOW/CUE/ISO import dialogs, and the shared setup download helper to `SetupDialogs.kt` mostly verbatim.
- [x] Rename any package-visible generic helpers only if needed to avoid same-package collisions.
- [x] Remove moved declarations from `SetupActivity.kt` and clean imports if safe.
- [x] Run focused diagnostics, JVM tests, scoped code-quality checks, and line-count/diff checks.
- [x] Update this plan and the survey plan with results.

## Notes
- Prefer an exact block move over manual rewriting to avoid line-count drift.
- Keep launcher state and ActivityResult wiring in `SetupActivity.kt`.

## Results
- Added `SetupDialogs.kt` with `SetManagementDialog`, GOG installer import, SOW archive import, CUE/BIN disc import, ISO import, the private dialog size formatter, and the shared setup download helper.
- Renamed the package-visible download helper to `setupDownloadFile` to avoid adding a generic same-package top-level helper name.
- Kept ActivityResult launchers, import selection state, and launcher orchestration in `SetupActivity.kt`.
- `SetupActivity.kt` was 6635 lines before this tranche and is 5079 lines after formatting. `SetupDialogs.kt` is 1641 lines, for about 85 net added lines from imports, formatter wrapping, and the retained Activity-local size formatter.
- Raw `git diff --numstat` is misleading while extracted files are untracked and the branch includes previous tranches; use before/after line counts for this split's line-count sanity check.
- Validation passed with focused diagnostics, `:app:testDebugUnitTest`, and scoped `android\run-code-quality.ps1 -Fix -Paths ...`.
