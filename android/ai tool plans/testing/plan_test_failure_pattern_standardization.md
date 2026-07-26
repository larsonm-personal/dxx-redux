# Test failure pattern standardization

## Goal

Survey the automated test suite for patterns fixed during the July 25-26
failure investigations. Apply preemptive fixes where the same risk exists and
centralize repeated behavior when a shared helper provides a clearer contract.

## Plan

- [x] Record the scope and preserve existing in-progress changes
- [x] Inventory launcher reset and manual game-button selection patterns
- [x] Inventory modal-screen transition waits and fixed-delay substitutes
- [x] Inventory unbounded ADB health and readiness probes
- [x] Inventory child-test infrastructure recovery patterns
- [x] Select narrow standardizations supported by repeated evidence
- [x] Implement shared helpers and migrate affected tests
- [x] Run focused unit, catalog, quality, and integration validation
- [x] Record adopted and rejected standardizations

## Findings

- Adopted: migrated all 21 automated scripts that launched a game through
  rendered launcher button taps to the existing `enter_game` boundary action
- Adopted: catalog validation now rejects launcher game-button taps in
  automated scripts while retaining the two explicit `_manual` tests
- Adopted: shared `Adb` and `Adb-Dev` calls now use their bounded variants
  with a 30-second default, preserve standard error, and allow an override
- Adopted: bounded ADB server restart, device enumeration, and multiplayer
  launcher log polling
- Retained: Obsidian's `active_wall_transition_count` wait is specific to
  entering automap, which pauses the wall simulation. Other trigger tests wait
  for their own route or progression state and do not share that hazard
- Retained: extraction infrastructure recovery stays at the suite boundary,
  where one complete-spec retry follows launcher recovery. Copying retry logic
  into individual extraction tests would hide state and partial-run failures
- Retained: raw bulk asset pushes need size-aware explicit timeouts and were
  not routed through the 30-second default

## Validation

- `test_validate_automation_catalog.ps1`
- `test_test_helpers_process_wait.ps1`
- `test_extract_suite_device_preflight.ps1`
- `test_extract_regression_workflow.ps1`
- Scoped `run-code-quality.ps1 -Fix` over all changed files
- Sequential device run: `test_double_launch.ps1`
- Sequential device run: `test_engine_prefs_unified.json5 -Game d1`
- Sequential device run: `test_engine_prefs_unified.json5 -Game d2`
- Sequential device run: `test_autoselect_crash_unified.json5 -Game d2`
- Successor device run with captured output:
  `test_pilot_long_hold_delete_unified.json5 -Game d1`
- `git diff --check`
