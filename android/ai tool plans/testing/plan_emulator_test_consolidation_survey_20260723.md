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
