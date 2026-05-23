# Plan: Levelcomplete Touch Skip Triage 2026-05-16

## Goal

- Reproduce `test_levelcomplete_touch_skip.json5`, fix only the current local failure, and confirm the rerun.

## Local hypothesis

- The script is likely asserting a brittle suppress-window detail after the first swallowed tap instead of the stable behavior the engine guarantees.

## Cheap check

- Compare the failing assertion against `android_input.c` to see whether the suppress flag is expected to remain active after counting a swallowed tap.

## Steps

- [x] Reproduce `test_levelcomplete_touch_skip.json5` from the fresh report details
- [x] Confirm the local suppress-window semantics in native input code
- [x] Fix only the brittle script assertion
- [x] Rerun `test_levelcomplete_touch_skip.json5` to confirm the outcome
- [x] Update this plan with the final result

## Result

- The native suppressor counts swallowed touches but does not guarantee `cutscene_tap_suppress_active` remains true after the first swallowed tap.
- Trimmed the post-tap assertion to the stable guarantees: the touch was counted and the level-complete page stayed open.
- Fresh rerun passed on emulator with `cutscene_tap_suppress_hits = 1` and the second tap advancing as expected.