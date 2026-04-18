# Metl154 tranche 6: two-pass cull cleanup

## Scope

This tranche resumes Part C.3 cleanup with a stronger regression safety rail:

- remove the Android-only forced `glDisable(GL_CULL_FACE)` tweak from the
  GPU two-pass merged-wall fallback path in D1 and D2
- keep the stronger stock regression green and re-validate the dedicated
  force-two-pass probe after updating its expected state

Out of scope for this tranche:

- changing the stock 64x64 `auto_old_texmerge` fallback route
- removing polygon offset from the two-pass path
- changing merged-wall snapshot ranking or coverage assertions

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `android\run_test.ps1 -ScriptName game_scripts\test_merged_wall_two_pass_probe.json5 -Game d2 -Install`
4. `android\run_test.ps1 -ScriptName game_scripts\test_merged_wall_snapshot_regression.json5 -Game d2 -Install`

## Work items

- [x] Remove forced cull disable from the D1 and D2 two-pass path
- [x] Update the two-pass automation probe to expect `force_cull_off=false`
- [x] Re-run the two-pass probe and the strengthened default regression
- [x] Update this file with findings and validation

## Findings

- The Android-only forced cull disable in the two-pass merged-wall path was
  removable in both D1 and D2 without breaking the force-two-pass probe.
- The dedicated two-pass probe continued to report the centered target face as
  `seg=83 side=3 face=1 submit_nv=5 route=force_two_pass merge_impl=gpu_two_pass`
  while `merged_wall_last_draw_state.force_cull_off` became `false`.
- The strengthened default regression still stayed on the stock-path invariant:
  the target face remained `route=old_texmerge merge_impl=auto_old_texmerge`
  with `decision_reason=tri13_face1_stock_64x64`.

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `android\run_test.ps1 -ScriptName game_scripts\test_merged_wall_two_pass_probe.json5 -Game d2 -Install`: passed
- `android\run_test.ps1 -ScriptName game_scripts\test_merged_wall_snapshot_regression.json5 -Game d2 -Install`: passed

## Status

- [x] In progress
- [x] Validation complete

