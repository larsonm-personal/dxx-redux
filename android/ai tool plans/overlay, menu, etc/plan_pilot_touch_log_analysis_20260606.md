# Pilot Touch Log Analysis Plan

Created: 2026-06-06

## Request

Analyze phone logs from the newest build where taps on the pilot select list did not pick a pilot at all, then use the diagnostics to identify and fix the touch region failure.

## Plan

1. [done] Read the attached Game Logs and extract the Java, native, SDL push, and listbox touch records around failed pilot taps.
2. [done] Compare logged coordinates, scale rects, and listbox hit bounds to identify the coordinate-space mismatch.
3. [done] Patch the Android touch mapping or listbox bounds logic in both D1 and D2 as needed.
4. [done] Run scoped code quality and Android build validation, then update this plan with results.

## Notes

- Failed pilot taps reached native/listbox handling, so the SurfaceView and native remap path were not the immediate problem.
- The failed rows logged `item=-1` even though their mapped engine coordinates were inside or just below the last pilot row's logged bounds.
- The Android pilot-listbox shortcut used `rel_y / LINE_SPACING`, but `LINE_SPACING` is global font/canvas-scale dependent and does not match selected row overlap/height. The fix now reuses `listbox_get_item_bounds()`.

## Validation

- `android\helpers\stop-stale-formatters.ps1`: no stale formatter tasks found.
- `android\run-code-quality.ps1 -Fix -Paths ...`: passed.
- `android\gradlew.bat -p android :app:assembleDebug`: passed with existing Gradle deprecation warnings.
