# Plan: report_20260518_223317 last three failures

## Goal
- Fix the three non-passing cases from `temp/test_reports/report_20260518_223317.md`

## Failures
- `test_fire_primary` timeout after launcher flow reaches movie escape dispatch
- `test_quick_record_classic_sidecar_install` fail in SetupActivity flow before a result is reported
- `test_launcher_dpad` fail because `DPAD_CENTER` on initial focus does not enter Define Controls

## Hypotheses
- `test_launcher_dpad`: initial launcher focus or key routing changed, so the test assumption is stale or the launcher no longer treats the focused card as activate-on-center
- `test_quick_record_classic_sidecar_install`: the setup automation flow likely changed around quick-record install state or result-file reporting
- `test_fire_primary`: the scripted route likely stalls on a launcher or movie transition edge and needs either a small script fix or a corresponding engine/input routing fix

## Steps
- Read the three failing test scripts and the nearest helper/implementation files they exercise
- Form one local hypothesis and one cheap discriminating check for each failure
- Apply the smallest code or test fix that addresses the owning behavior
- Run targeted validation for each touched slice instead of a full suite rerun

## Validation
- `test_quick_record_classic_sidecar_stage.json5` rerun: pass
- `test_quick_record_classic_sidecar_install.json5` rerun: pass after switching to direct `install_staged_demo`
- `android/tests/test_launcher_dpad.ps1` rerun: initial test-side focus wait failed, which confirmed the launcher was not reporting an initial focused button
- `android/gradlew.bat :app:installDebug` with `JAVA_HOME=c:\local\jdk-21`: pass after keyboard-mode focus fix in `SetupActivity.kt`
- `android/tests/test_launcher_dpad.ps1` rerun against rebuilt app: pass
- `android/run_test.ps1 -ScriptName test_fire_primary.json5 -Game d2`: pass on current build; `skip_briefing` advanced through movie and briefing, and the held-fire assertion completed

## Outcome
- `test_quick_record_classic_sidecar_install` fixed by removing the stale `Add to Game` tap and using the existing direct staged-demo install helper after an Advanced-page smoke check
- `test_launcher_dpad` fixed by making the launcher request keyboard input mode before reapplying initial focus, then keeping the test honest by waiting for `Define Controls` to actually report focus before sending `DPAD_CENTER`
- `test_fire_primary` no longer reproduces on the current rebuilt app and passed targeted validation without an additional code change
- Follow-up note: `test_quick_record_classic_sidecar_stage.json5` still relies on substring button matching for `Descent 2` and currently lands on `Launch Descent 2`; it did not block this tranche, but it remains a small launcher-automation hardening candidate