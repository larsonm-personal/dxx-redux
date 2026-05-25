# TouchOverlayView helper split - 2026-05-24

## Goal
Reduce `TouchOverlayView.kt` by moving the existing top-level pure helper groups into focused same-package files while keeping the custom `View` class behavior unchanged.

## Plan
- [x] Confirm the top-level helper boundaries and test coverage for admin tray policy, weapon labels, remaining actions, and mouse acceleration.
- [x] Move admin tray and controller-menu policy helpers to `AdminTrayPolicy.kt`.
- [x] Move weapon wheel labels, button labels, active indicators, and drag-zone helper policy to `WeaponWheelLabels.kt`.
- [x] Move remaining touch action discovery and labels to `RemainingTouchActions.kt`.
- [x] Move mouse acceleration history and multiplier helpers to `TouchMouseAcceleration.kt`.
- [x] Keep `TouchOverlayView.kt` as the drawing/input `View` implementation and avoid changing runtime logic.
- [x] Run focused diagnostics, JVM tests, scoped code-quality checks, and line-count checks.
- [x] Update this plan and the survey plan with results.

## Notes
- This should be a mechanical split of the prefix before `class TouchOverlayView`.
- Keep helper visibility and names unchanged unless diagnostics require a narrow cleanup.
- Use before/after line counts because new untracked files make raw diff stats look like pure additions.

## Results
- Added `AdminTrayPolicy.kt`, `WeaponWheelLabels.kt`, `RemainingTouchActions.kt`, and `TouchMouseAcceleration.kt` from the existing top-level helper prefix.
- Kept `TouchOverlayView.kt` as the drawing/input `View` implementation. No runtime logic was intentionally changed.
- Widened only the helpers still shared with `TouchOverlayView.kt`: `CONTROLLER_MENU_FOCUS_COLOR`, `defaultWeaponWheelSlotLabel`, `laserWheelLabel`, and `currentBombName` are now `internal`.
- `TouchOverlayView.kt` was 4591 lines before this tranche and is 3912 lines after formatting. The four helper files total 687 lines, for about 8 net added lines.
- Converted restored non-ASCII decorative comments in `TouchOverlayView.kt` to plain ASCII while preserving the original comment meaning.
- Validation passed with focused diagnostics, `:app:testDebugUnitTest` before and after formatting, ASCII scan, and scoped `android\run-code-quality.ps1 -Fix -Paths ...`.
