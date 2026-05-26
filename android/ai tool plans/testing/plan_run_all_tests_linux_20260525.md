# Run all tests on fresh Linux VM

## Plan
- [x] Inspect `run_all_tests.ps1` and its infrastructure helpers for Linux assumptions
- [x] Run the test suite and capture the first blocking failure
- [x] Patch Linux/emulator/test-runner compatibility issues as they appear
- [ ] Re-run until the suite runs to completion and report failures separately from infrastructure issues

## Current pass
- [x] Run focused no-infrastructure tests on Linux to expose the next script/runtime blocker
- [x] Patch the narrowest Linux compatibility issue found
- [ ] Re-run the focused test and update this plan with the result

## Notes
- `emu_health.ps1` still had Windows-only `adb.exe` and `emulator.exe` paths; patched to use `test_host_platform.ps1`
- `Start-Process -WindowStyle` is Windows-only; patched emulator launch to set it only on Windows
- User enabled `/dev/kvm`; unwound the no-KVM API 23/software-emulation workaround and returned AVDs to API 34 x86_64
- `get_linux_android_prereqs.sh` now installs the normal system host build/test toolchain: C/C++ compiler, make, CMake, Ninja, pkg-config, Cargo, and Rust
- After system packages were installed, `test_cue_iso` exposed a stale CMake cache from the removed Zig experiment; fix scripts to regenerate CMake build dirs that reference missing tools
- `test_fpcalc_and_acoustid` found only `chromaprint_info.json5` metadata in music dirs, so make it skip when no actual MP3 payload is present
- Gradle unit tests exposed a Linux case-sensitivity bug in nested disc import hoisting; preserve the existing root filename when replacing a smaller root game file
- Consolidate Linux base prerequisite setup into the existing Linux/Ubuntu setup script so the Android bootstrap, native desktop builds, extract helpers, Rust server tests, and input-demo tests use one package list
- Host input-demo replays now resolve D1/D2 data from `game_data/game_data_index.txt`, so extracted GOG data works without copying files into `game_data_to_copy_to_emulator/temp`
- Desktop OpenGL shader compile failed on Linux because the D1/D2 tex2 shader emitted a GLES-only `precision` qualifier on non-Android; remove that qualifier for desktop GL
- Android launcher resume selection could pick same-second progress autosaves over exit autosaves after aborting a game; add a save-kind tie-breaker and cover it in native metadata tests
- `test_saf_archiver` used a PowerShell text pipeline for binary `adb shell cat` fallback; resolve `descent2.ham` from the hash index and avoid text-mode binary copies
- `test_all_extracts` on Linux failed every direct CD/ISO spec during app-private source staging; initial quoting fix exposed zero-byte/partial `adb exec-in dd` writes, so app-private staging now uses `adb push` to `/data/local/tmp` followed by quoted `run-as cat` into the app-private source path
- `run_all_tests.ps1` Tier 3 recovery referenced `$emu1Serial` before initialization after an extract test failure; initialize it from the online emulator list before running extract tests
- `test_extract.ps1` direct import broadcasts used unescaped adb shell paths; switch setup command broadcasts to device-shell-quoted argv, recursively recreate set dirs during cleanup, wait for emulator restarts, and let non-passing/no-prior direct imports settle as skip when they do not produce launch files
