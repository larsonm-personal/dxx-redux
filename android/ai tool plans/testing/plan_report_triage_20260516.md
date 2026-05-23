# Report Triage Plan 2026-05-16

- [x] Read the current full-suite report and separate intro-path failures from the other failures
- [ ] Patch the remaining single-emulator intro-path scripts that still launch straight into the first menu wait
- [x] Rerun representative intro-path tests and repair any remaining local mismatches
- [x] Reorder the unattended suite so non-emulator and faster tests run before single-emulator tests, with dual-emulator tests last
- [x] Inspect the obvious non-intro failures from the report and fix the local root cause where it is clear and cheap
- [x] Update this note with the validated set and any remaining follow-up

## Intro-path batch

- `test_autoselect_crash_unified.json5`
- `test_engine_prefs_unified.json5`
- `test_fire_primary.json5`
- `test_launch_to_automap.json5`
- `test_launcher_graphics_debug_prefs.json5`
- `test_menu_scale_d2.json5`
- `test_merged_wall_snapshot_regression.json5`
- `test_merged_wall_two_pass_debug_mode_probe.json5`
- `test_merged_wall_two_pass_probe.json5`
- `test_quick_record_classic_sidecar_stage.json5`
- `test_resolution_unified.json5`
- `test_reticle_options_stage_d2.json5`
- `test_saf_basic.json5`
- `test_gog_installer_redbook_unified.json5`

## Non-intro follow-up

- `test_quick_record_classic_sidecar_install`
- `test_launcher_dpad`
- `test_skip_every_launch_button_manual_unified`
- `test_extract`
- `test_gog_installer_d1_unified`

## Validated in this tranche

- `test_launcher_dpad` passes after moving the initial controller-page focus request into the composed controller section lifecycle
- `test_extract` passes after switching the post-launch flow to automation scripts and repairing `test_extract_regression_template.json5`
- `test_all_extracts` default-mode smoke passes on the repaired extract path
- `test_gog_installer_d1_unified` passes after fixing launcher `assert_button` off-screen handling and repairing the D1 game-phase script
- `test_gog_installer_redbook_unified` passes on the current build without additional code changes
- `test_pause_menu_return` passes after fixing `STEP_KEY` in `android/app/src/main/cpp/shared/game_automate.cpp` so direct in-game key dispatch pre-arms its sent state before nested event loops can redispatch the same key
- `test_abort_game_to_main_menu_d2` passes with the same automation fix; the temporary extra wait before the first `Esc` was removed after validation

## Remaining follow-up

- Rerun the still-unchecked intro-path scripts from the report, especially `test_autoselect_crash_unified`, and fix only the next local failure that still reproduces
- Revisit any remaining single-emulator failures that now look unrelated to intro startup after the shared `Esc` automation fix
- Return to dual-emulator and multiplayer items after the single-emulator queue is clear