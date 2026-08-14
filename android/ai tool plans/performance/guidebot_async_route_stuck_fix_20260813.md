# Guide-Bot async route stuck fix

## Goal

Find and fix the indefinite `No ... in mine` Guide-Bot response after route
metadata was moved off the level-load path

## Work

- [x] Trace the message and async readiness transition on device and in code
- [x] Fix polling and goal selection without enabling async simulation changes
- [x] Add focused regression coverage and diagnostics
- [x] Run scoped formatting, tests, D1/D2 builds, and emulator verification

## Findings

- Async readiness returned `ESCORT_GOAL_UNSPECIFIED`, but two AI-frame callers
  still passed that value into path creation. The legacy failure message then
  indexed the goal label table with `-2` and repeatedly reset the same state.
- A launcher worker cancelled during the handoff to the game could terminate
  the game request. Kotlin stopped after that non-busy failure while native
  readiness remained `calculating`, so no later cache poll could recover.
- Background completion now carries a request generation so a result from a
  previous level cannot change the current level's readiness.
- Cold-cache emulator testing exposed an uninitialized unexplored-route result
  and the cached-metadata path leaving live replanning disabled. The result is
  now initialized and live replanning is enabled only after a valid cache has
  been loaded.

## Validation

- Scoped code quality passed.
- Android `assembleDebug` and the route precompute ordering unit test passed.
- Windows D1 and D2 builds passed.
- `test_escort_goal_policy` passed through CTest.
- A cold D2 route-cache run of `test_guidebot_unexplored_goal.json5` passed all
  44 steps with route readiness `complete`.
- The full Android unit suite reached 789 tests with one unrelated failure in
  `ModManagerMissionZipTest.missionRarImportsReetusAndStagesAtMissions` because
  SevenZip native initialization failed.
