# Test Cleanup + Controller Compare Unification

## Phase 1: Cleanup (done)
- [x] Delete old non-unified json5: test_resolution.json5, test_autoselect_crash_repro.json5, test_gog_installer_redbook.json5
- [x] Delete thin wrapper ps1: test_resolution_unified.ps1, test_autoselect_crash_unified.ps1
- [x] Delete old ps1: test_resolution.ps1, test_autoselect_crash.ps1, test_gog_installer_redbook.ps1
- [x] Keep test_gog_installer_redbook_unified.ps1 (has GOG exe push logic)
- [x] Update run_all_tests.ps1: testTimeouts, add TimeoutSeconds to json5 discovery
- [x] Update json5 header comments (remove refs to deleted ps1 wrappers)

## Phase 2: Unify test_controller_compare (done)
- [x] Add `assert_controller_match` action to LauncherScriptExecutor.kt
  - Reads controller_introspect.json (launcher dump) and introspect.json (game dump)
  - Compares joystick_controls sections: control_type exact, summary fields as warnings, items case-insensitive on name
  - Fails with mismatch detail if they differ
- [x] Create test_controller_compare_unified.json5
  - enter_launcher, reset_state, write_default_config, setup_command patch_pilots
  - setup_command controller_introspect (game=${GAME})
  - enter_game ${GAME}
  - select pilot, wait for menu, introspect
  - enter_launcher
  - assert_controller_match
- [x] Delete old test_controller_compare.json5 and test_controller_compare.ps1
- [x] Run test + code quality -- both D1 and D2 pass
- [x] Bugfix: reset_state now also deletes controller_introspect.json (prevents stale file issues)
- [x] Bugfix: added write_default_config step after reset_state (reset_state deletes controller_config.json)

## Analysis: Tests that remain as ps1 (cannot easily unify)
- test_double_launch.ps1: needs HOME key + background/resume lifecycle
- test_saf_archiver.ps1: needs file move/copy shell operations
- test_autoselect_plx.ps1: file format validation (not game automation)
- test_extract.ps1: template-based extraction testing
- Infrastructure tests: test_cue_iso, test_server_integration, test_bot_client, etc.
