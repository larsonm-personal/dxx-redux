# Plan: Comprehensive Test Suite Fixes

## Status: Phases 1-3, 5a-5c DONE, Phase 4 manual, Phase 6 draft ready

## Completed

### Phase 1: Quick Wins / Reverts
- [x] 1a. Reverted test_controller_compare.json5 regression -- restored `when`-gated
  select "Ok" for D2 + key "enter" for D1 (was collapsed to a single key enter)
- [x] 1b. Centralized pilot deletion into Reset-GameState in test_helpers.ps1
  Updated callers: run_test.ps1, test_resolution.ps1 (both phases), test_autoselect_crash.ps1
- [x] 1c. Fixed test_resolution Phase 2 descent.cfg -- now writes AspectX=3/AspectY=4
  alongside ResolutionX=640/ResolutionY=480 (was missing aspect ratio values)

### Phase 2: False Crash Detection
- [x] 2a. Increased Test-EmulatorHealthy timeout from 5s to 10s
- [x] 2b. Added retry logic in Watch-AutomationResult health check -- waits 5s and
  retries once before declaring crash (was single-shot failure)

### Phase 3: Controller Compare D1 Support
- [x] 3a. Added KC_JOY_META_D1 (48 entries) to SetupActivity.kt mirroring d1/main/kconfig.c
  Renamed KC_JOY_META -> KC_JOY_META_D2 for clarity
- [x] 3b. Made writeControllerIntrospectJson game-aware -- accepts `game` parameter,
  selects correct metadata + byte array per game. Broadcast handler passes --es game extra.
- [x] 3c. Updated test_controller_compare.ps1 to pass --es game $gameId when requesting
  launcher controller introspection

### Phase 5a: get_cmake.sh
- [x] Implemented get_cmake.sh following get_jdk.sh pattern
- [x] Added CMAKE_URL to tool_versions.conf

### Phase 5b-c: Infrastructure
- [x] 5b. Added -AutoServer switch to run_all_tests.ps1 -- auto-builds and starts
  matchmaking server, cleans up on exit. Unblocks test_bot_client in unattended mode
- [x] 5c. Added -AutoServer switch to test_bot_client.ps1 -- same pattern: auto-build,
  start, wait for port 9000, cleanup on exit

### Phase 6: Documentation
- [x] 6a. Draft ready at android/ai tool plans/testing/draft_copilot_instructions_additions.md
  Covers: terminal pollution, health checks, pilot cleanup, game data, descent.cfg format,
  D1/D2 differences, json5 automation, server auto-start

## TODO (manual / requires emulator)

### Phase 4: Game Data Provisioning
- [ ] 4a. Copy D2 game data from game_data/extracted/VERTIGO/ to game_data_to_copy_to_emulator/data/
  Required files: DESCENT2.HOG, DESCENT2.HAM (minimum for gameplay)
  Then run push_game_data.sh to push to emulator
- [ ] 4b. Verify test_saf_archiver.ps1 works with descent2.ham available
- [ ] 4c. Verify test_extract.ps1 auto-discovers specs from game_data/CD images/

### Emulator verification
- [ ] Run test suite on emulator: .\run_all_tests.ps1 -AutoServer

## Files Changed (11 files, +281/-56)
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt (+75: D1 KC_JOY_META, game-aware introspection)
- android/game_scripts/test_controller_compare.json5 (reverted to when-gated steps)
- android/get_deps/get_cmake.sh (implemented from TODO)
- android/get_deps/tool_versions.conf (+CMAKE_URL)
- android/run_all_tests.ps1 (+60: -AutoServer switch)
- android/run_test.ps1 (use Reset-GameState)
- android/test_helpers.ps1 (+25: Reset-GameState, health check retry, timeout increase)
- android/tests/test_autoselect_crash.ps1 (use Reset-GameState)
- android/tests/test_bot_client.ps1 (+81: -AutoServer switch)
- android/tests/test_controller_compare.ps1 (game-specific introspection)
- android/tests/test_resolution.ps1 (Reset-GameState + AspectX/Y in Phase 2 cfg)

## Verification
- Kotlin compiles: BUILD SUCCESSFUL (gradlew compileDebugKotlin)
- PS1 syntax: all 5 modified scripts parse OK
- ktlint: SetupActivity.kt passes with no new warnings
- No d1/ or d2/ source changes
