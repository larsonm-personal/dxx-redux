# Metl154 tranche 7: two-pass polygon offset cleanup

## Scope

This tranche continues the Android merged-wall pass cleanup using the stronger
regression and the maintained force-two-pass probe:

- remove the Android-only forced polygon-offset tweak from the GPU two-pass
  merged-wall fallback path in D1 and D2
- keep the stronger stock regression green and re-validate the dedicated
  force-two-pass probe after updating its expected state

Out of scope for this tranche:

- changing the stock 64x64 `auto_old_texmerge` fallback route
- changing merged-wall snapshot ranking or coverage assertions
- deleting the debug-mode-only depth suppression path

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `android\run_test.ps1 -ScriptName game_scripts\test_merged_wall_two_pass_probe.json5 -Game d2 -Install`
4. `android\run_test.ps1 -ScriptName game_scripts\test_merged_wall_snapshot_regression.json5 -Game d2 -Install`

## Work items

- [ ] Remove forced polygon offset from the D1 and D2 two-pass path
- [ ] Update the two-pass automation probe to expect `force_polygon_offset=false`
- [ ] Re-run the two-pass probe and the strengthened default regression
- [ ] Update this file with findings and validation

## Status

- [ ] In progress
- [ ] Validation complete
