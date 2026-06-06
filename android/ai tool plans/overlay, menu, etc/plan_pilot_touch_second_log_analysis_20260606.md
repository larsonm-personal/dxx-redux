# Pilot Touch Second Log Analysis Plan

Created: 2026-06-06

## Request

Analyze logs from the new build where pilot select still has incorrect tap locations, then fix the remaining coordinate or hit-test mismatch.

## Plan

1. [done] Read the latest attached Game Logs and extract failed pilot select touch sequences.
2. [done] Compare Java, native remap, SDL push, and listbox hit-test values against logged listbox row bounds.
3. [done] Patch the remaining Android touch/listbox mismatch in both D1 and D2.
4. [done] Run scoped code quality and Android build validation, then update this plan with results.

## Notes

- The new logs showed real pilot items were hit, but the row geometry was inconsistent: `box` height was 176 while consecutive logged row starts were only 12 px apart.
- `listbox_draw_contents()` used live `LINE_SPACING` inside the row loop, and each loop iteration changes the active font. That let drawing and hit testing inherit different spacing from whatever font was current.
- The fix stores `line_spacing` in the listbox structure at creation time, then uses that stored value for row drawing, bounds, scaled Android drawing, and listbox touch diagnostics.

## Validation

- `android\helpers\stop-stale-formatters.ps1`: no stale formatter tasks found.
- `android\run-code-quality.ps1 -Fix -Paths ...`: passed.
- `android\gradlew.bat -p android :app:assembleDebug`: passed with existing Gradle deprecation warnings.
