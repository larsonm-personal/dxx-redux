# Emulator Test Consolidation Survey

## Goal

Identify emulator-based tests that can share one launcher and game session while
preserving their current checks, with emphasis on folding launcher checks into
the preamble of compatible in-level tests.

## Plan

- [x] Read repository instructions and the 2026-07-23 passing test report
- [x] Map emulator tests to launcher state, game, mission, level, and cleanup
- [x] Identify clean merge groups and ordering constraints
- [x] Estimate setup savings and rank the proposed groups
- [x] Deliver a read-only consolidation proposal

## Scope

- Do not modify test scripts or harness behavior in this survey
- Preserve all existing assertions in any proposed combination
- Prefer combinations that do not require restoring contaminated persistent state
- Treat user changes already present in the worktree as out of scope

## Findings

- The report contains 61 passing JSON scenarios with 54:19 of recorded test
  time.
- A small launcher-only scenario normally costs about 8 to 15 seconds even
  when its own actions take about one second.
- A fresh launcher-to-level path normally costs about 20 to 25 seconds before
  the scenario-specific checks.
- `run_test.ps1` force-stops the app after every standalone JSON scenario.
  Launcher scenarios also reset state, relaunch SetupActivity, clear save
  files, stage a unique script, and start a new automation watcher.
- The 3:24:26 suite wall time is not a clean consolidation benchmark. The
  `test_obsidian_level2_route` log contains a 2:03 host or emulator suspension
  between automation steps. Its durable automation result reports only 23.456
  seconds of active script time.

## Recommended clean groups

1. Launcher and graphics runtime
   - Keep `test_ogl_runtime_texture_options_unified` as the owner.
   - Move in `test_button_discovery`,
     `test_debug_log_refresh_button`,
     `test_launcher_graphics_debug_prefs`, and
     `test_msaa_fbo_smoke_d2` as D2-only phases.
   - Order: launcher button checks, Advanced refresh, enable and assert debug
     prefs, write MSAA config, launch, assert debug and MSAA state, exercise
     runtime texture options, return to launcher, restore prefs.

2. Launcher MIDI preview and in-game music
   - Move `test_midi_preview_hmp_unified` into the D2 launcher preamble of
     `test_music_track_controls_unified`.
   - The tests use the same base assets and have no conflicting state.

3. Shared menu rendering scenario
   - Keep `test_newmenu_render_paths_unified` as the owner.
   - Reuse its first fresh-pilot launch for `test_keyboard_viewport`.
   - Reuse its main-menu and listbox phases for `test_menu_scale_d2`,
     `test_joystick_menu`, and `test_reticle_options_stage_d2`.
   - Reuse its loaded level for `test_pause_menu_return`,
     `test_pause_menu_viewport_d2`, `test_readable_tiny_help_d2`, and
     `test_controls_readability_d2`.
   - Run destructive or terminal menu navigation last.

4. Shared input gameplay scenario
   - Keep `test_axis_mapping` as the owner.
   - Add the TAB default-binding checks from `test_keyboard_defaults` after
     axes and buttons have returned to neutral.
   - Add D2-only primary and secondary fire phases from `test_fire_primary`
     and `test_fire_secondary`. Write the default config before launch, run the
     primary hold before the secondary ammo-count check, and release every
     injected input before moving to the next phase.

5. D2 Counterstrike level 7
   - Merge `test_d2_level7_bitmap_cache` into
     `test_d2_level7_reactor_water_profile`.
   - Assert the cache smoke state after the existing five-second settle, then
     pose the reactor view and run the 22-second profile hold.

6. D2 Counterstrike level 1 merged-wall graphics
   - Merge `test_merged_wall_snapshot_regression` and
     `test_door45_cover_gpu_regression`.
   - Run the normal metl154 snapshot first, then change the texture target and
     pose for door45. Both already clear robots and use the same merge mode.

7. Obsidian level 1 objectives
   - Merge `test_obsidian_level1_objective_markers` and
     `test_obsidian_level1_next_objectives`.
   - Run the initial all-overlay and Guide-Bot assertions first. Then switch
     to the remaining overlay, apply the idempotent key and trigger updates,
     and run the final remaining-objective assertions.

8. KCXF2 level 5 route
   - Keep `test_kcxf2_guidebot_route_next` as the owner.
   - It already covers the same blue-key, trigger 6, red-key, trigger 10
     progression as `test_kcxf2_level5_guidebot_route`, and continues farther.
   - Move only the unique automap overlay counts and pending-path assertions
     into the longer route-next scenario.

9. D2 level-complete and mine-exit touch suppression
   - Merge `test_levelcomplete_touch_skip` and
     `test_mine_exit_movie_touch_skip`.
   - Run the synthetic carried-touch level-complete phase first, return to
     gameplay, then trigger the real endlevel branch and assert its suppress
     arm.

10. Launcher metadata
    - Fold base D2 metadata analysis into a base D2 scenario preamble.
    - Fold imported Obsidian metadata checks into the combined Obsidian level
      1 scenario.
    - Keep the parameterized Plutonia check as a small standalone case until
      there is a Plutonia in-level consumer, or combine the three D2 metadata
      checks into one launcher-only metadata batch.
    - Keep Lunar Series separate because it clears mods and imports a D1-only
      archive without a compatible in-level scenario.

## Lower-confidence follow-up

- The three Counterstrike level 1 Guide-Bot scenarios can probably share one
  level in the order cage release, HUD layout, unexplored goal. Validate that
  the cage-release phase does not change the target selection assumed by the
  later phases before combining them.
- KCXF2 and Obsidian multi-level scenarios could reuse one archive import and
  app process, aborting to the main menu between levels. This would create very
  long scenarios with more state coupling, so it should follow the smaller
  same-level merges rather than lead them.

## Keep isolated

- Save, autosave, resume, pilot deletion, autoselect, and quick-record
  scenarios intentionally mutate persistent state.
- Crash, double-launch, SAF, extraction, mission ZIP batch, random preview,
  LAN, and multiplayer tests use special PowerShell orchestration or external
  Android activities.
- Intro, title-music, engine-pref, resolution, and mod-loading scenarios use
  relaunches as part of their actual contract and should not be combined merely
  because they visit the launcher.

## Expected impact

- The small same-level and semantic pairs should remove about 3 to 5 minutes
  with low implementation risk.
- The menu and input groups remove roughly 15 additional fresh game starts.
  Based on the current 20 to 25 second launch cost, they should save another 4
  to 6 minutes.
- A reasonable first target is 7 to 10 minutes from the 54:19 JSON portion of
  an awake, healthy full run. Re-measure after the first tranche because the
  report's two-hour suspension makes its total wall time unsuitable as a
  baseline.

## First Implementation Tranche

Scope selected on 2026-07-23:

- [x] Merge the two KCXF2 level 5 route scenarios
- [x] Merge the two Counterstrike level 7 scenarios
- [x] Merge the two Counterstrike level 1 merged-wall scenarios
- [x] Merge the two Obsidian level 1 objective scenarios
- [x] Merge launcher MIDI preview into the shared in-game music scenario
- [x] Merge the two D2 endlevel touch-suppression scenarios
- [x] Remove the six absorbed standalone scripts
- [x] Run the automation catalog validator and scoped code quality
- [x] Run every combined scenario on the emulator
- [x] Record final validation results

Implementation constraints:

- Preserve every assertion from the absorbed scripts
- Preserve the stronger timeout values from either source
- Keep phase log messages so a combined failure identifies its former test
- Run state-mutating phases only after assertions that require a fresh level
- Do not change unrelated test or product code in this tranche

Validation results:

- Automation catalog valid with 58 standalone JSON tests, 18 support scripts,
  and 53 PowerShell entries
- Scoped code quality passed for all six owner scripts and this plan
- KCXF2 level 5 route owner passed on the emulator
- Counterstrike level 7 reactor and bitmap-cache owner passed on the emulator
- Counterstrike level 1 merged-wall and door45 owner passed on the emulator
- Obsidian level 1 objective owner passed on the emulator
- Unified music owner passed for both D1 and D2; the D2 run included the MIDI
  preview preamble
- Level-complete and mine-exit touch-suppression owner passed on the emulator
- A structural comparison against the six absorbed scripts confirmed that
  every former assertion leaf remains in its owner

## Second Implementation Tranche

Scope selected on 2026-07-23:

- [x] Merge launcher button discovery into the OGL runtime owner
- [x] Keep the Advanced debug-log refresh check standalone after isolated
  reruns showed its off-screen assertion is not currently composable
- [x] Merge launcher graphics debug preference checks into the OGL runtime owner
- [x] Merge the D2 MSAA FBO smoke check into the OGL runtime owner
- [x] Do not land the investigated launcher scroll-helper change; neither
  accessibility action moved the Advanced Compose page
- [x] Remove the three absorbed standalone scripts
- [x] Preserve the existing D1 owner path
- [x] Run catalog, assertion-preservation, and scoped quality checks
- [x] Run the combined owner for D1 and D2 on the emulator
- [x] Record final validation results

Planned D2 phase order:

1. Reset launcher state and perform button discovery
2. Enter Advanced and exercise the immediate debug-log refresh action
3. Enable and assert the video-info, graphics-log, and texture-log preferences
4. Write `MsaaLevel=2` to both D2 config locations
5. Launch D2 once and assert texture logging and MSAA at the main menu
6. Load level 1, assert the MSAA gameplay render path, and run the existing
   runtime texture-option sequence
7. Return to the launcher and restore all three debug preferences

Empirical exception:

- `test_debug_log_refresh_button` passed in the 2026-07-22 full report, but
  its unchanged standalone script now fails because `assert_button` cannot
  scroll the Advanced Compose page to `Refresh`
- The same assertion fails when embedded in the owner, both before and after
  trying the directional accessibility scroll action first
- Keep this test standalone rather than weakening or silently dropping its
  pre-click and post-click button assertions

Validation results:

- Structural comparison confirmed that every assertion leaf and button
  assertion from the three absorbed scripts remains in the owner
- Automation catalog valid with 55 standalone JSON tests, 18 support scripts,
  and 53 PowerShell entries
- Scoped code quality passed
- Android debug APK assembly and debug unit tests passed during the scroll
  investigation; the final source-matching APK also assembled successfully
- D2 owner passed 66 of 66 steps in 52.204 seconds
- D1 owner passed 42 of 42 steps in 40.153 seconds
- Final diff contains no launcher automation helper change

## Third Implementation Tranche

Scope selected on 2026-07-23:

- [x] Merge keyboard viewport coverage into the owner's fresh-pilot launch
- [x] Merge D2 menu-scale assertions into the first main-menu phase
- [x] Merge joystick-menu navigation into the post-listbox Options phase
- [x] Merge the D2 reticle scroll-box assertion into the Options phase
- [x] Merge pause-menu return assertions into the owner's in-level pause phase
- [x] Keep D2 readable tiny-help standalone because its direct-render assertion
  conflicts with the owner's 3:4, 1280x960 config
- [x] Remove the five absorbed standalone scripts
- [x] Keep `test_pause_menu_viewport_d2` and
  `test_controls_readability_d2` out of this owner because they deliberately
  write a conflicting 9:16, 1280x720 config
- [x] Run structural, catalog, quality, and emulator validation
- [x] Record final validation results

Planned phase order:

1. Launch with the owner's 3:4, 1280x960 config
2. Exercise soft-keyboard pilot entry, type `test`, and accept the pilot
3. Assert D2 main-menu scaling
4. Relaunch and assert the existing pilot listbox and newmenu render paths
5. Exercise joystick customization for D1/D2 and reticle options for D2
6. Load level 1 and combine the pause-menu render and return assertions

Deferred compatible pair:

- `test_pause_menu_viewport_d2` and `test_controls_readability_d2` share the
  same 9:16, 1280x720 config and loaded D2 level
- Combining them separately preserves their explicit narrow-viewport contract
  instead of silently changing this owner's 3:4 render-path coverage

Empirical configuration exception:

- The combined D2 emulator run reached the readable tiny-help assertion but
  reported `menu_scale.direct_render=false` under the owner's 3:4, 1280x960
  config
- The standalone test's `direct_render=true` assertion is preserved unchanged
  instead of weakening its viewport-specific contract

Validation results:

- Structural comparison confirmed that every assertion leaf from the five
  absorbed scripts remains in the owner
- The restored `test_readable_tiny_help_d2` matches its HEAD version
- Automation catalog valid with 50 standalone JSON tests, 18 support scripts,
  and 53 PowerShell entries
- Scoped code quality and `git diff --check` passed
- D1 owner passed 64 of 64 filtered steps in 14.826 seconds
- D2 owner passed 76 of 76 filtered steps in 30.924 seconds
- No APK rebuild was required because this tranche changes JSON automation and
  the plan only; emulator validation used the source-matching installed APK

## Fourth Implementation Tranche

Scope selected on 2026-07-23:

- [x] Merge keyboard-default TAB and automap coverage into `test_axis_mapping`
- [x] Merge the D2 primary-fire hold coverage into `test_axis_mapping`
- [x] Merge the D2 secondary-fire tap coverage into `test_axis_mapping`
- [x] Remove the three absorbed standalone scripts
- [x] Run structural, catalog, quality, and emulator validation
- [x] Record final validation results

Planned phase order:

1. Reset launcher state and write the default config for D2
2. Create a fresh pilot and load Counterstrike level 1 on Trainee for D2 or
   First Strike level 1 on Rookie for D1
3. Run the existing analog-axis, D-pad, and trigger-axis routing coverage,
   releasing every injected input
4. Press TAB, assert that the default keyboard binding opens the automap, and
   dismiss it with Escape
5. For D2, hold and release primary fire, then wait for its live control state
   to return to neutral
6. For D2, assert the default secondary binding and seven-missile starting
   count, tap secondary fire once, and assert that exactly one missile was used

Compatibility constraints:

- Preserve every expectation from all three absorbed scripts
- Keep the secondary-fire phase last because it intentionally mutates ammo
- Run the primary-fire phase first because it does not consume secondary ammo
- Use Trainee only for D2 so the exact seven-to-six ammo contract is preserved
- Keep D1 on the owner's existing Rookie path
- Use game-selecting `enter_game` actions because the owner's former
  page-dependent `Descent N` button sequence fails when SetupActivity resumes
  on its configuration page

Validation results:

- Semantic multiset comparison confirmed that every expectation from the three
  absorbed scripts remains in the owner
- Automation catalog valid with 47 standalone JSON tests, 18 support scripts,
  and 53 PowerShell entries
- Scoped code quality and `git diff --check` passed
- D1 owner passed 81 of 81 filtered steps in 25.711 seconds
- D2 owner passed 97 of 97 filtered steps in 40.447 seconds
- The D2 run confirmed primary-fire neutralization and the exact secondary
  missile transition from seven to six
- No APK rebuild was required because this tranche changes JSON automation and
  the plan only; emulator validation used the source-matching installed APK

## Fifth Implementation Tranche

Scope selected on 2026-07-23:

- [x] Merge base D2 launcher metadata analysis into the Obsidian level-1 owner
- [x] Merge direct Obsidian archive metadata analysis into the same owner
- [x] Remove the two absorbed standalone metadata scripts
- [x] Keep the parameterized Plutonia metadata script standalone
- [x] Keep Lunar Series metadata standalone
- [x] Run action-preservation, catalog, quality, and emulator validation
- [x] Record final validation results

Planned phase order:

1. Enter the launcher with base D2 data and a directly provisioned
   `obsidian.zip`
2. Run the former base D2 all-level metadata analysis
3. Run the former paired base and Obsidian direct-archive analyses
4. Reset state and clear mods so direct analysis cannot affect gameplay setup
5. Import Obsidian through the owner's existing mission ZIP path
6. Run the owner's existing imported metadata analysis and level-1 objective
   coverage

Compatibility constraints:

- Preserve the exact metadata action parameters and minimum level counts
- Provision the archive in both the direct-analysis and mission-import
  locations because those paths exercise different launcher behavior
- Keep Plutonia standalone because it remains parameterized and has no
  compatible in-level consumer
- Keep Lunar Series standalone because it is D1-only and intentionally clears
  and imports a different mod set

Validation results:

- Semantic comparison confirmed that every non-preamble action from the two
  absorbed metadata scripts remains unchanged in the owner
- Automation catalog valid with 45 standalone JSON tests, 18 support scripts,
  and 53 PowerShell entries
- Scoped code quality and `git diff --check` passed
- The first emulator attempt passed every new metadata phase, then hit the
  owner's pre-existing final objective timing failure when trigger 8 remained
  pending
- The unchanged rerun passed 65 of 65 steps in 26.440 seconds, including base
  D2, direct Obsidian archive, imported Obsidian, and all objective assertions
- No APK rebuild was required because this tranche changes JSON automation and
  the plan only; emulator validation used the source-matching installed APK

## Sixth Implementation Tranche

Scope selected on 2026-07-23:

- [x] Merge D2 pause-menu viewport coverage into
  `test_controls_readability_d2`
- [x] Remove the absorbed pause-menu viewport script
- [x] Run expectation-preservation, catalog, quality, and emulator validation
- [x] Record final validation results

Planned phase order:

1. Write the shared 9:16, 1280x720 configuration and load Counterstrike level 1
2. Open the pause menu and assert its logical scaled viewport
3. Close the pause menu and wait for the game window to return to the front
4. Open the in-game options and Controls menu
5. Exercise keyboard, joystick, mouse, and weapon-key editor scaling
6. Verify weapon-key paging and the bottom of the Controls scroll box

Compatibility constraints:

- Preserve every expectation from both scripts
- Keep the pause phase first because it returns cleanly to an unmodified level
- Explicitly close the pause menu before opening the in-game options
- Preserve the shared 9:16 render-path contract rather than moving these checks
  into the incompatible 3:4 newmenu owner
- Use `enter_game` so launch does not depend on the current SetupActivity page

Validation results:

- Semantic multiset comparison confirmed that every pause-menu expectation
  remains in the controls-readability owner
- Automation catalog valid with 44 standalone JSON tests, 18 support scripts,
  and 53 PowerShell entries
- Scoped code quality and `git diff --check` passed
- Combined D2 owner passed 58 of 58 steps in 25.791 seconds
- No APK rebuild was required because this tranche changes JSON automation and
  the plan only; emulator validation used the source-matching installed APK
