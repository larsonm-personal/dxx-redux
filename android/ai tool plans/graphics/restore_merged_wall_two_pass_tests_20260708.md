# Restore merged wall two-pass tests plan

## Goal
- Restore the two merged-wall two-pass coverage scripts deleted during stale-field cleanup.
- Preserve their general texture-merge regression intent while aligning with current debug plumbing.
- Avoid disturbing unrelated dirty workspace changes.

## Steps
- [x] Re-read current merged-wall native debug route support and the deleted test expectations.
- [x] Restore or adapt the missing debug control needed by the tests.
- [x] Restore the two JSON5 scripts with current field names and assertions.
- [x] Run scoped formatting/static checks.
- [x] Run focused verification that the tests are discoverable/parseable and that affected unit tests still pass.

## Verification
- Scoped `android\run-code-quality.ps1 -Fix -Paths ...` passed.
- JSON5 parse check passed for the touched game scripts.
- `rg` found no live `merged_wall_experiment`, `MERGED_WALL_EXPERIMENT`, or `force_legacy_merged_wall_texmerge` references under live Android scripts/app/test code.
- `.\gradlew.bat :app:assembleDebug` passed with JDK 21.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ResumeSavePanelTest --tests com.dxxredux.app.SettingsChildOverlayControllerTest` passed.
- `android\helpers\run_test.ps1 test_merged_wall_two_pass_probe.json5 -Game d2 -Install -TimeoutSeconds 420` passed on emulator.
- `android\helpers\run_test.ps1 test_merged_wall_two_pass_debug_mode_probe.json5 -Game d2 -TimeoutSeconds 420` passed on emulator.
