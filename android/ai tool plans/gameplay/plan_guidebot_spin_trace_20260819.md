# Guide-Bot Spin Investigation Plan

## Scope

- Reproduce or isolate the Guide-Bot spinning while navigating to an energy center
- Inspect energy-center goal selection, path creation, path following, and collision recovery
- Add an opt-in Android diagnostic trace if static analysis does not identify a safe fix

## Work

- [x] Trace the energy-center goal from command dispatch through movement
- [x] Identify existing debug configuration and exportable logging hooks
- [x] Implement a focused, rate-limited Guide-Bot navigation trace and confirmed fixes
- [x] Add introspection and automated coverage for the diagnostic behavior
- [x] Run scoped formatting, D2 build/tests, Android build, and focused device validation

## Findings

- Android's path readiness guard rejected `ESCORT_GOAL_UNSPECIFIED` before
  `escort_create_path_to_goal` could replace it with the active special goal.
  Energy-center commands therefore remained selected but never received a goal path.
- One-point `AIM_GOTO_OBJECT` paths are degenerate for the companion path follower and
  can make it repeatedly turn around one point. They now use the existing fallback,
  while valid two-point paths to nearby doors remain intact.
- Added an opt-in `Guide-Bot` debug-log category with command, path, once-per-second
  navigation, and suspected-spin records. The launcher can export these logs.

## Verification

- Scoped code-quality pass completed successfully for all changed files
- Windows D2 build completed successfully
- D2 CTest passed 40/40 tests
- Android `testDebugUnitTest assembleDebug` completed successfully for all ABIs
- On-device `test_kcxf2_guidebot_route_next.json5` passed 182/182 steps
- The device trace confirmed the energy-center command created goal 7's path to
  segment 319 with path length 40 before the remaining route scenarios passed
