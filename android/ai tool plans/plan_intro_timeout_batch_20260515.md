# Intro Timeout Batch Plan 2026-05-15

- [x] Confirm the current suite failures share the same cold-launch timeout shape
- [x] Patch launcher-backed scripts that launch the game and immediately wait for `screen_mode=menu`
- [x] Rerun the affected scripts and fix any remaining local mismatches
- [x] Update this note with the final validated set

## Current candidate failures

- `test_keyboard_defaults.json5`
- `test_joystick_menu.json5`
- `test_axis_mapping.json5`
- `test_dpad_triggers.json5`
- `test_door45_pose_repro.json5`
- `test_door45_cover_gpu_regression.json5`
- `test_death.json5`
- `test_controller_compare_unified.json5`

## Working hypothesis

- These scripts still assume a cold launch reaches `screen_mode=menu` directly after tapping `Launch Descent ...`
- On the current emulator/data set, the intro movie/title path can appear first, so the fix is script-local launcher reset plus explicit `intro_active` and `skip_intro` handling before the first menu wait

## Validated set

- `test_keyboard_defaults.json5` (`d2`)
- `test_joystick_menu.json5` (`d2`)
- `test_axis_mapping.json5` (`d2`)
- `test_dpad_triggers.json5` (`d2`)
- `test_death.json5` (`d2`)
- `test_controller_compare_unified.json5` (`d2`)
- `test_keyboard_viewport.json5` (`d2`, `d1` spot-check)
- `test_door45_pose_repro.json5` (`d2`)
- `test_door45_cover_gpu_regression.json5` (`d2`)

## Follow-up note

- The `door45` scripts needed one extra launcher-local repair after `reset_state`: tap `Descent 2` before `Launch Descent 2`, because the launcher came back focused on D1 and hid the D2 launch button