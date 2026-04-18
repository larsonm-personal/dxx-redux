# Metl154 tranche 8: two-pass debug depth cleanup

## Scope

This tranche continues Part C.3 cleanup after the cull and polygon-offset
removals:

- remove the remaining Android-only debug-mode depth suppression from the GPU
  two-pass merged-wall path in D1 and D2
- add a targeted automation probe that exercises `merged_wall_mode=alpha` so
  the surviving debug-only state change has explicit coverage
- keep the maintained stock regression green on the accepted rock331 visibility
  safety rail

Out of scope for this tranche:

- changing the stock 64x64 `auto_old_texmerge` fallback route
- changing merged-wall snapshot ranking or coverage assertions
- launcher or in-game debug UI work from Parts C.1 and C.2

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `android\run_test.ps1 -ScriptName test_merged_wall_two_pass_probe.json5 -Game d2 -Install`
4. `android\run_test.ps1 -ScriptName test_merged_wall_two_pass_debug_mode_probe.json5 -Game d2 -Install`
5. `android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2 -Install`

## Work items

- [x] Remove debug-mode depth suppression from the D1 and D2 two-pass path
- [x] Add a targeted debug-mode automation probe for the alpha visualization path
- [x] Re-run the maintained two-pass probe, the new debug-mode probe, and the stock regression
- [x] Update this file with findings and validation

## Findings

- The remaining Android-only debug-mode depth suppression in the GPU two-pass
  merged-wall path was removable in both D1 and D2 without breaking either the
  maintained two-pass probe or a dedicated alpha-debug-mode probe.
- The maintained force-two-pass probe continued to report the centered target
  face as `seg=83 side=3 face=1 submit_nv=5 route=force_two_pass`
  `merge_impl=gpu_two_pass` with `force_cull_off=false`,
  `force_polygon_offset=false`, and `force_depth_off=false`.
- The new debug-mode probe proved that `merged_wall_mode=1` no longer needs a
  special depth-state override to keep the two-pass debug visualization alive.
- The stock rock331 regression remained green with the target face still on
  `route=old_texmerge merge_impl=auto_old_texmerge` and
  `decision_reason=tri13_face1_stock_64x64`.

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `android\run_test.ps1 -ScriptName test_merged_wall_two_pass_probe.json5 -Game d2 -Install`: passed
- `android\run_test.ps1 -ScriptName test_merged_wall_two_pass_debug_mode_probe.json5 -Game d2 -Install`: passed
- `android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2 -Install`: passed

## Status

- [x] In progress
- [x] Validation complete