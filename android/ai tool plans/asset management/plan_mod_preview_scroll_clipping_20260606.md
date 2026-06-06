# Mod Preview Scroll Clipping Plan

## Goal
- [x] Fix mission ZIP constituent previews that show clipped last lines and do not become scrollable when content barely exceeds the visible area.

## Steps
- [x] Inspect the mod details preview composables and identify the scroll/height constraint causing the clipping.
- [x] Apply a small layout fix that preserves the existing detail dialog design.
- [x] Add or update focused coverage if the affected layout has practical test hooks.
- [x] Run scoped code quality and a focused Android test/build check.

## Notes
- `MissionZipConstituentDialog` was using an unbounded plain `Column`, so nested metadata could be clipped by the platform dialog without exposing scroll.
- `MetadataContentsBox` also had tight bottom padding, which made descenders on the last visible line easy to clip at small heights.
- No direct JVM coverage was added because the affected composables are private UI layout code without an existing test harness; `assembleDebug` verifies Compose compilation for the changed path.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/SetupSections.kt','android/ai tool plans/asset management/plan_mod_preview_scroll_clipping_20260606.md')` passed.
- `.\android\gradlew.bat -p android :app:assembleDebug` passed.
