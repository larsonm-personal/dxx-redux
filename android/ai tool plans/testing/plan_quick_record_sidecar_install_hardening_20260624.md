Quick Record Sidecar Install Hardening - 2026-06-24

Goal:
- Harden the remaining failure from report_20260623_230322.md without weakening the test assertions or relying on longer waits.

Target:
- test_quick_record_classic_sidecar_install

Plan:
- [done] Inspect the failing log, script, and related quick-record sidecar helper flow.
- [done] Identify the missing state signal or cleanup boundary that made the install run fail without a useful script result.
- [done] Patch the smallest script or automation helper change that makes the behavior fault tolerant.
- [done] Run a focused install test and any adjacent quick-record sidecar test that could regress.
- [done] Record verification and residual risk.

Notes:
- The report failure was a cascade: `test_quick_record_classic_sidecar_stage` failed before producing a staged D2 demo, so `test_quick_record_classic_sidecar_install` found no fixture.
- The install test still intentionally depends on the stage test instead of duplicating the long recording setup.
- Hardened the install half so it now waits for and asserts the staged demo triplet before installing, then asserts the installed input demo, RNG trace, and classic `.dem` sidecar after install.
- Added setup introspection for installed input demos in the active file set's `demos` directory.

Verification:
- Scoped code quality passed for the touched Kotlin package and script.
- `assembleDebug` passed with JDK 21.
- Installed the debug APK with `adb install -r`.
- `test_quick_record_classic_sidecar_stage.json5 -Game d2` passed and produced a staged demo.
- `test_quick_record_classic_sidecar_install.json5 -Game d2` passed after the stage run.
- Final setup introspection showed `staged_input_demo_count` as 0 and `installed_input_demos[0]` as `quick_record_sidecar_install.dximdemo` with readable header, RNG trace, and classic demo sidecar.

Residual risk:
- The install test remains a dependent second half of the sidecar pair by design. If stage fails, install should still fail, but now the failure points at the missing staged fixture before attempting install.
