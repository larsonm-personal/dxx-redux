# Test suite speed fixed-wait audit 20260612

## Goal
Find fixed pauses and other low-risk suite slowdowns that can be replaced with
condition-based waits while preserving test coverage.

## Plan
- [x] Inventory fixed waits, post delays, and runner sleeps across Android tests
- [x] Rank candidates by likely wall-time impact and risk
- [x] Patch clear script-level waits first, preferring existing introspection waits
- [x] Run focused validation for touched tests
- [x] Run scoped code quality for touched files
- [x] Record remaining speedup candidates for later passes

## Findings
- Script-level fixed time totals before the first patch:
  - `wait_ms`: 80550 ms across maintained JSON5 scripts.
  - `post_delay_ms`: 138950 ms across maintained JSON5 scripts.
  - Game-tab `tap_button` post delays: 30000 ms.
- The safest repeated candidate was the 1000 ms delay after tapping
  `Descent ${GAME_NUM}`, `Descent 1`, `Descent 2`, or
  `${GAME_SELECT_BUTTON_TEXT}` in launcher scripts.
- The next launch-button `tap_button` already polls until the exact launch
  button exists and is enabled, so the tab delay can be zero without removing
  the readiness check.
- A second pass removed 500 ms waits immediately before difficulty selection.
  `select` already polls for `Rookie`, `Trainee`, or `Insane` when a
  `timeout_ms` is present, so these waits duplicated the existing condition.
- Current fixed-time totals after both patches:
  - `wait_ms`: 65550 ms.
  - `post_delay_ms`: 108950 ms.

## Validation
- `test_launch_to_automap.json5` passed for D1 and D2 after removing both
  the launcher tab delay and the pre-difficulty wait.
- `test_engine_prefs_unified.json5` passed for D1 and D2 after removing both
  the repeated launcher tab delays and the pre-difficulty wait.
- `test_intro_skip_inputs_unified.json5` passed for D1 and D2.
- `test_fire_primary.json5` passed after removing the pre-difficulty wait.

## Remaining candidates
- `test_skip_every_launch_button_manual_unified.json5` still carries a large
  intentional 30000 ms manual-observation wait; skip or split this before
  optimizing automated suite time.
- Redbook/audio installer tests still have several 2000-4000 ms post delays.
  These should be replaced with audio/redbook introspection checks rather than
  shortened blindly.
- `test_mod_loading.json5` still has a 5000 ms wait after game entry; a
  condition based on `mounted_mods` or another mod-ready introspection field
  would be the safer next pass.
- `test_quick_record_classic_sidecar_stage.json5` still has recorder/sidecar
  waits; replace them only after adding or using an explicit recorder-ready
  signal.

## Introspection-backed pass plan
- [ ] Inspect the runner and automation actions for existing condition support
- [ ] Replace one or two high-value fixed waits with explicit readiness checks
- [ ] Add narrow introspection fields only if an existing field is not enough
- [ ] Validate each touched script on the emulator
- [ ] Run scoped code quality and record final results
