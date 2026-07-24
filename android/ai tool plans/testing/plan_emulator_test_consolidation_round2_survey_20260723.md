# Emulator Test Consolidation Round 2 Survey

## Goal

Review the first successful full run after the initial consolidation work and
identify additional tests that can share emulator setup, launcher work, or
gameplay state without weakening coverage.

## Plan

- [x] Capture the new full-suite runtime baseline
- [x] Rank the remaining emulator tests by runtime and setup cost
- [x] Compare compatible launcher, game, mission, and viewport flows
- [x] Identify clean combinations and tests that should remain standalone
- [x] Estimate the likely savings and recommend an implementation order

## Baseline

- Report: `report_20260723_161642.md`
- Passed: 86
- Failed: 0
- Skipped: 7
- Total time: 00:53:35
- Timed test bodies: 00:51:25
- Remaining standalone JSON scenarios: 32 tests, 00:26:30

The suite is now split almost evenly between JSON emulator scenarios and host,
extraction, SAF, network, and multiplayer work. Further JSON consolidation has
useful but diminishing returns.

## Recommended combinations

### 1. Pilot, controller, intro, and engine preferences

Owner: `test_engine_prefs_unified`

Absorb:

- `test_controller_compare_unified`
- `test_intro_skip_inputs_unified`
- `test_title_music_skip_pref_unified`

Current measured total: 03:55

Clean phase order for each game:

1. Create the pilot and use the launch for the existing touch intro-skip check
2. Patch the real pilot controller data
3. Use the controller readback launch for the controller-A intro-skip check
4. Compare controller data in the launcher and restore the default config
5. Enable intro auto-skip, launch, and assert both the preference and title
   music request
6. Restore the intro preference, write engine preferences, launch, and verify
   them in a real level

These four tests currently perform 18 D1/D2 launches. The combined flow needs
about eight because the touch and controller-A checks can reuse launches that
the controller and engine-preference contracts already require. This is the
largest clean remaining opportunity.

Estimated saving: 01:10 to 01:40

Risk: medium. The controller fixture must be restored before engine preference
verification, but the existing controller test already performs that restore.

### 2. Counterstrike level 1 navigation and abort lifecycle

Owner: `test_guidebot_unexplored_goal`

Absorb:

- `test_secret_reveal_automap_d2`
- `test_abort_game_to_main_menu_d2`

Current measured total: 01:42

Phase order:

1. On the pristine level, run secret reveal automap assertions
2. Disable secret reveal and close the automap
3. Run the existing Guide-Bot cage, HUD, and unexplored-route phases
4. Abort the game last and verify the level-1 auto-abort save in the launcher

All three tests use default graphics, Counterstrike level 1, and a fresh D2
pilot. The secret phase does not alter world progression, and abort is already
terminal.

Estimated saving: 00:35 to 00:50

Risk: medium-low. Set `show_resume_offer=true` before the shared launch and
explicitly restore the secret reveal flag before starting Guide-Bot checks.

### 3. Launch, background/resume, and terminal death

Owner: `test_launch_to_automap`

Absorb: `test_death`

Current measured total: 01:57

After each existing D1/D2 background-resume phase, abort to the main menu,
select the death test's original level and Insane difficulty, apply the same
normal damage path, and leave death last. This reuses the pilot and running
game process while preserving the original level numbers and death assertions.

Estimated saving: 00:25 to 00:35

Risk: low. Death is terminal and does not contaminate earlier assertions.

### 4. Android game requests and quick recording

Owner: `test_android_saveload_dispatch_unified`

Absorb: `test_quick_record_classic_sidecar` in the D2 branch only

Current measured total: 01:44

Run all save/load request assertions first. In D2, start and stop quick
recording in the already-loaded level, then enter the launcher and preserve
all staged triplet, install, RNG trace, and classic-demo assertions. Quick
recording is terminal and needs no clean gameplay state beyond a loaded level.

Estimated saving: 00:15 to 00:25

Risk: medium-low. Keep the manual save checks before recording and retain the
existing staged-demo and crash-report cleanup.

### 5. Narrow and default menu readability

Owner: `test_controls_readability_d2`

Absorb: `test_readable_tiny_help_d2`

Current measured total: 01:01

The earlier attempt to run tiny-help assertions under the owner's 9:16 config
correctly failed. Keep the existing narrow-viewport phase unchanged, then
return to the launcher, reset the config, launch a fresh default viewport, and
run the F1 tiny-help assertions as a second phase. This preserves both render
contracts while sharing dependency staging and the outer automation session.

Estimated saving: 00:10 to 00:20

Risk: low. Do not reuse the narrow viewport for the tiny-help phase.

### 6. Counterstrike level 2 switch completion

Owner: `test_levelcomplete_touch_skip`

Absorb: `test_counterstrike_level2_switch_completion`

Current measured total: 00:47

The level-complete test already advances naturally from level 1 into level 2.
Insert the switch-completion Guide-Bot assertions after the level-2 briefing
and before the terminal mine-exit trigger.

Estimated saving: 00:12 to 00:18

Risk: low. The switch phase may mutate keys and triggers, but the following
mine-exit assertion only requires a loaded level.

### 7. Graphics resolution preamble

Owner: `test_ogl_runtime_texture_options_unified`

Absorb: `test_resolution_unified` in the D2 branch

Current measured total: 01:35

Run the default and explicit 640x480 resolution launches first. Return to the
launcher, reset the config, then run the existing MSAA, debug preference, and
runtime texture phases unchanged.

Estimated saving: 00:10 to 00:15

Risk: low. The explicit config reset is mandatory so 640x480 does not leak
into the existing OGL contract.

### 8. D1 mission metadata preamble

Owner: `test_trine2_d1_in_d2_custom_textures`

Absorb: `test_lunar_series_revamped_metadata_only`

Current measured total: 01:10

Analyze the Lunar Series archive first, clear imported mods, then preserve the
existing Trine import, metadata, and in-level custom-texture flow.

Estimated saving: 00:07 to 00:10

Risk: low. Keep an explicit `clear_mods` boundary between archives.

## Suggested implementation order

1. Counterstrike level 2 switch completion
2. Launch plus death
3. Android dispatch plus quick recording
4. Counterstrike level 1 navigation plus abort
5. Menu readability with a fresh-config boundary
6. Preference and controller owner
7. OGL resolution preamble
8. Lunar metadata preamble

The first five give good savings with simple state boundaries. The preference
owner has the largest payoff but deserves its own tranche because it changes
the ordering of several relaunch contracts.

## Keep standalone

- `test_debug_log_refresh_button`: prior combination attempts showed its
  off-screen Advanced-page assertion is not composable with the current
  launcher automation scrolling
- `test_pilot_long_hold_delete_unified`,
  `test_autosave_resume_missing_pilot_unified`, and
  `test_autoselect_crash_unified`: deletion, missing-pilot recovery, and crash
  selection are their central persistent-state contracts
- `test_mod_loading`, `test_merged_wall_snapshot_regression`, and
  `test_d2_level7_reactor_water_profile`: special assets, graphics state, or a
  deliberate 22-second profile hold dominate their runtimes
- KCXF2 and Obsidian owners: they already reuse one import across four or five
  levels and are long enough for useful failure localization
- GOG installer, extraction, mission ZIP batch, SAF, random preview, LAN, and
  multiplayer tests: their external orchestration is the behavior under test
- `test_level_metadata_launcher_zip_reusable`: retain its parameterized
  launcher-only role rather than burying the currently single Plutonia option
  in an unrelated owner

## Expected impact

Implementing all eight groups removes ten standalone JSON tests. Based on the
new report and the number of avoided launcher-to-game paths, the likely suite
saving is 03:00 to 04:10, reducing a comparable healthy full run to roughly
49:25 to 50:35.

## Constraints

- Preserve every existing assertion and deliberate configuration boundary
- Prefer combinations with a clean menu or launcher handoff
- Avoid grouping tests only because both happen to use an emulator
- Keep destructive, relaunch, extraction, and multiplayer contracts isolated
  unless their setup and cleanup requirements are genuinely compatible
