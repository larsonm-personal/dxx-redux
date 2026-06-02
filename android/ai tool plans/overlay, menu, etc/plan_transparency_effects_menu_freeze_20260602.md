# Transparency effects in-game menu freeze plan - 2026-06-02

## Goal
Prevent the Android in-game Graphics Options `Transparency Effects` toggle from triggering the black-screen texture reload/stall path.

## Steps
- [x] Read project instructions and locate the Android graphics option bridge plus D1/D2 in-game menu handlers.
- [x] Identify the reload trigger: Android menu options already live-apply through the graphics bridge, but `graphics_config()` still unconditionally calls `gr_set_attributes()` and `gr_set_mode()` on exit.
- [x] Skip the legacy full OGL mode reset on Android for D1 and D2 graphics menu exit.
- [x] Keep launcher/runtime native option behavior intact so startup-applied pilot defaults and live texture filtering still use their existing paths.
- [x] Run focused searches/build checks and mark this plan complete.

## Verification
- `.\run-windows-build.ps1` passed for D1 and D2. Existing `weapon.c` C4715 warnings remain.
- `C:\local\android-sdk\cmake\3.31.6\bin\ctest.exe --test-dir buildd1 --output-on-failure` found no registered tests.
- `C:\local\android-sdk\cmake\3.31.6\bin\ctest.exe --test-dir buildd2 --output-on-failure` found no registered tests.
- With JDK 21, `.\gradlew.bat :app:externalNativeBuildDebug` passed for `arm64-v8a`, `armeabi-v7a`, and `x86_64`.
- `git diff --check` passed for the touched files.
