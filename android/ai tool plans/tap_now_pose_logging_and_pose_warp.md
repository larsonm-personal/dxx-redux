# Tap-now pose logging and pose warp

## Goal

- extend the Video Info overlay `mwall snap: Tap` path so the on-device debug log captures a reproducible ship pose together with the tapped merged-wall snapshot request
- include any extra merged-wall or texture state that is likely useful for diagnosing a default-texture door corruption case
- add a matching automation/game-control warp path if an exact pose-and-heading replay helper does not already exist

## Plan

- [x] trace the current Video Info overlay snapshot trigger and native merged-wall snapshot request logging
- [x] define a stable pose logging format that includes position and heading and is easy to paste into a regression script
- [x] implement the extra tap-now logging and any additional useful texture/debug fields
- [x] add a matching automation/game-control pose warp helper if exact pose replay is not already supported
- [x] run code quality and targeted diagnostics on the touched files, then update this note with findings

## Notes

- existing automation already supports `face_view` for face-relative positioning, but the new request needs a direct position+heading replay path for logs captured by tapping a corrupted face on-device
- tap-now snapshot requests now flow through the shared `android_merged_wall_request_snapshot()` helper for both overlay taps and automation `set_debug`
- the new pose log emits a paste-ready JSON snippet of the form `{"action":"pose_view",...}` with `segment`, `x`, `y`, `z`, `pitch`, `bank`, and `heading`
- snapshot completion now logs per-face base and overlay texture metadata including stock-vs-png state, dimensions, UV max values, mipmap state, and mask handle, in addition to the existing face and portal logs
- `android\run-clang-format.ps1 -Fix` followed by `-Check` passed after the native edits
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat bundleDebug` passed after fixing a malformed switch block in `game_automate.cpp` and adding `ogl_init.h` for the new texture metadata logger
- the umbrella `android\run-code-quality.ps1 -Fix` wrapper is still blocked in this environment because `PSScriptAnalyzer` is not installed
- attempted integration-test follow-up was blocked because `C:\local\android-sdk\platform-tools\adb.exe devices` reported no attached authorized device
