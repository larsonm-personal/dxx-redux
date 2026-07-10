# Guidebot unexplored wheel goal

## Goal
Expose the existing largest-contiguous-unexplored-area route goal in the
guidebot command wheel and ensure selecting it uses the same intermediate route
planning as the end-of-level goal.

## Plan
- [x] Trace the unexplored goal enum, route refresh, command dispatch, and wheel
  item construction.
- [x] Add the missing wheel option at the narrowest UI/command ownership point.
- [x] Verify unavailable, selected, multiplayer-owner, and route-refresh behavior
  still follow existing guidebot goal conventions.
- [x] Add or extend focused integration coverage for wheel visibility and goal
  selection.
- [x] Run scoped quality checks, D2 native tests/build, and the focused Android
  automation test.

## Findings
- The native feature was already complete. `META_GUIDE_FIND_UNEXPLORED` dispatches
  to `escort_find_unexplored_goal`, which selects the largest contiguous
  unexplored component and asks the shared metadata route planner for the route.
- The generated route retains the normal end-of-level dependencies. In the
  Counterstrike level 1 integration fixture, the first selected step is the
  reactor and the terminal step is the unexplored component.
- The stock Guide wheel intended to expose `Unexplored` as its center action,
  but the center had no distinct visual control. Its label was repeatedly drawn
  directly over the wheel slices and was easy to miss as an actionable option.
- Layout migration version 8 only added the action when the Guide wheel center
  was blank. A layout already marked version 8 but missing the action was never
  repaired, and a custom center action prevented `Unexplored` from being added
  anywhere.
- Multiplayer intent synchronization and owner handoff already preserve the
  unexplored target mode; no native or network changes were needed.

## Changes
- Bumped the touch layout schema to version 9 and rerun an idempotent Guide wheel
  repair for existing installations.
- Blank Guide wheel centers now receive the stock `Unexplored` action. Guide
  wheels with a custom center retain that action and receive an `Unexplored`
  slice before `Next` instead.
- Open radial menus now draw center actions as a distinct circular button with
  selected-state feedback.
- Added regression coverage for version-8 layouts missing the action and for
  custom-center layouts.

## Validation
- Focused `GuidebotLockedWheelTest` and `GyroToggleConfigTest`: passed.
- Full `:app:testDebugUnitTest`: passed.
- `:app:assembleDebug`: passed.
- `run-windows-build.ps1 -Target d2`: passed.
- D2 CTest: 14/14 passed.
- `test_guidebot_unexplored_goal.json5`: passed all 30 emulator steps. It
  confirmed target mode `unexplored`, a nonempty component, reactor-first route
  progression, and periodic route refresh.
- Scoped `run-code-quality.ps1 -Fix`: passed.
