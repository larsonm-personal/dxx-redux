# Touch Stick Initial Position Travel Plan

## Goal
- Research whether touch stick travel is limited by where the initial touch lands inside a control zone
- If confirmed, fix the behavior so drag distance is limited only by the initial touch spot and screen bounds

## Phase 1: Study
- [done] Inspect floating stick and axis region touch math
- [done] Identify whether the reported behavior is in stick, axis region, or both
- [done] Choose the smallest behavior-preserving fix

## Phase 2: Implementation
- [done] Patch touch travel math if a real limit is found
- [done] Add focused unit tests for the travel math
- [done] Update this plan with completed work

## Phase 3: Verification
- [done] Run focused Kotlin tests
- [done] Run scoped code quality on changed files

## Findings
- Standard floating analog sticks are not limited by the floating zone after touch start
  - On touch down, the stick stores `floatingCX/floatingCY` from the initial touch
  - On move, `updateStickFromTouch()` uses the active pointer position even outside the floating zone
  - Full stick value depends on initial touch, stick radius, and screen edge
- `AxisRegionControl` was zone-scaled
  - It captured the initial touch position, but computed full travel using the remaining distance to the control region edge
  - The active pointer could continue outside the region, so this made the region bounds an artificial scaling limit
- The fix changes axis-region scaling to use the full overlay view bounds
  - Vertical regions scale against `0..height`
  - Horizontal regions scale against `0..width`
  - The value still clamps at `-1..1`

## Verification Notes
- [done] `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.TouchAxisRegionTravelTest`
- [done] `.\android\run-code-quality.ps1 -Fix -Paths ...`
