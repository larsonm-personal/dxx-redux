# Single-device Android suite

Goal: run the existing basic single-emulator suite on Retroid Pocket 4 Pro and
share manual target selection between emulator and physical-device runners

- [x] Add a shared target picker with explicit serial and unattended defaults
- [x] Route single-device provisioning, recovery and child processes to that target
- [x] Use the isolated diagnostic app on physical devices to preserve installed games
- [x] Add runner integration coverage and document invocation
- [x] Build and run the basic single-device suite on Retroid JYPR42510121028
- [x] Run scoped quality checks and catalog tests; record results

Keep dual-emulator testing outside this change. Preserve the user's existing
edit to android/outstanding_bugs.md

## Evidence so far

- Retroid `JYPR42510121028`, Android 13; isolated app `com.dxxredux.app.nsdtest`
- Existing diagnostic files/preferences backed up to
  `android/temp/retroid-single-device-backup.tar` before the first test reset
- Actual terminal picker selected the Retroid by model/serial; unit integration
  covers multiple phones, unauthorized/offline filtering, explicit serials,
  child inheritance, broadcast scoping and physical preflight/recovery
- `test_launch_to_automap` passed D1 and D2, including background/resume checks:
  `temp/retroid-single-smoke/report_20261003_223856.md`
- Initial smoke discovered unnecessary ADB reconnect followed by an immediate
  online check; physical runs now keep the healthy transport
- Device initially asleep; preflight now uses ordinary wake/keyguard dismissal
  and asks for normal unlock if the keyguard is secure
- A concurrent controller-settings build changed Gradle outputs during the
  first instrumentation build. Rebuilt after it finished; suite APKs are now
  copied to their report directory and inherited by child install helpers
- Android debug build passed for both engines/all ABIs, including instrumentation
- Catalog validation passed: 285 top-level entries, 363 support scripts
- Scoped quality, target integration, existing process-wait/preflight and
  runtime/profile-menu checks passed
- Initial audit exposed 21 host-only fixtures incorrectly classified as emulator
  tests; corrected their catalog requirements and added catalog assertions
- The initial 34-test audit was stopped at mission ZIP import after its logs
  proved a partial `Descent 2` button match clicked `Launch Descent 2` before
  automation handoff. An exact-match retry confirmed the current launcher has
  no game selector: it has separate launch buttons. Removed the obsolete
  selector step and made the actual launch-button match exact
- Guide-Bot touch fixture initially failed because the handheld controller
  disables the touch overlay by default. The fixture now explicitly enables
  its required overlay; the Retroid rerun passed all 39 steps
- Physical runners temporarily keep the plugged-in display awake and restore
  the original setting in cleanup. Verified direct runner restored `0`
- Fresh basic profile selected 15 actual device tests, seed 276. The latest
  suite report has 14 passes and the mission-batch preprocessing failure:
  `temp/retroid-single-final/report_20261003_234615.md`
- SAF failed when its runner sent raw JSONC to the strict native parser. It now
  uses the shared resolver and passed through level 1 with HAM data served by
  the SAF archiver
- Mission batch also resolves its generated script to strict JSON before
  launch. Its strict-mode caller exposed optional-property assumptions in the
  shared resolver; made the resolver's permissive JSON property reads local
  and added a strict-mode catalog regression. Fixed failure reporting to read
  PSCustomObject properties instead of calling a dictionary method
- Focused mission-batch rerun passed all three archives (D1 and D2 launches),
  with zero failures, skips or timeouts:
  `temp/retroid-single-final/report_20261004_000240.md`
- All 15 selected basic-profile tests now have passing device evidence across
  the 14-pass suite and the corrected mission-batch rerun. This is seed 276's
  basic selection, not exhaustive coverage of every rotating scenario
- Final stay-awake setting read back as its original `0`
- Scoped quality checks passed; both catalog validators and target-selection
  integration passed. The automation catalog includes the strict-mode resolver
  regression. Existing process-wait, extraction preflight and runtime sampling
  regressions also passed

## Run it

Interactive target picker (emulator first, then authorized physical devices):

```powershell
pwsh -File android/run_all_tests.ps1 -SingleDevice
```

Reproduce the validated selection on the Retroid:

```powershell
pwsh -File android/run_all_tests.ps1 -SingleDevice -Serial JYPR42510121028 -SampleSeed 276
```

Individual JSON automation uses the same target selection:

```powershell
pwsh -File android/helpers/run_test.ps1 test_launch_to_automap.jsonc
```
