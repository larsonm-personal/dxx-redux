# Android build failures 2026-05-08

## Goal
- fix the current Android native build failures blocking `0_upload_to_test.ps1`
- keep the fix narrow to the actual compile breakage first, then trim obvious new warnings if they are in the touched slice

## Steps
- [completed] confirm the direct cause of the Android compile errors in the failing D2 files
- [completed] apply the smallest code change that restores Android compilation
- [completed] rerun a focused Android native build validation
- [completed] record the result and any remaining warning follow-up

## Notes
- initial hypothesis: `input_demo_trace_player_shield_change()` was added to `input_demo_hooks.h`, but some D2 files now calling it do not include that header directly, and Android Clang is rejecting the implicit declaration
- cheap check: inspect the include blocks in `d2/main/gamecntl.c` and `d2/main/powerup.c` and compare with D2 files that already compile with the same helper
- confirmed cause: both failing D2 files called `input_demo_trace_player_shield_change()` but only included `input_demo_energy_trace.h`, not `input_demo_hooks.h`; Android Clang treated that as an implicit-declaration error
- implemented: added the direct `input_demo_hooks.h` include to `d2/main/gamecntl.c` and `d2/main/powerup.c`
- validation: direct Ninja rebuild of the existing arm64 `dxx-redux-d2` target succeeded, and `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]"` also succeeded when run with `JAVA_HOME=C:\local\jdk-21`
- remaining state: the Android arm64 native build still emits many pre-existing warnings, including the `gauges.c` unused-variable warnings from the user log, but they are no longer blocking the build
