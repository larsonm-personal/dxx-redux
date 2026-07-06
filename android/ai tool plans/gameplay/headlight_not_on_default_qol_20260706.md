# Headlight Default QoL

## Plan
- [x] Locate the QoL option definitions, persistence path, and headlight startup state.
- [x] Add a default-enabled QoL setting that keeps the headlight off by default, in both games if the plumbing is duplicated.
- [x] Run focused formatting/build checks and record the result here.

## Notes
- Added a default-enabled launcher Gameplay switch backed by `headlight_off_by_default`.
- D2 applies that QoL preference after reading pilot files, so existing pilots do not keep the headlight on by default while the QoL switch is enabled.
- The native D2 pilot preference read/write path also exposes `headlight_active_default` for export/import and automation.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths ...` passed for the touched files.
- `gradlew.bat :app:assembleDebug` passed with JDK 21.
- `gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.MainActivityInputTypeTest` passed.
- Full `:app:testDebugUnitTest` was also run, but two unrelated `GyroToggleConfigTest` migration tests failed.
- `git diff --check` passed.
