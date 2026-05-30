# MSAA FBO Helper Extraction - 2026-05-30

## Goal
- Move duplicated Android MSAA FBO lifecycle logic out of D1/D2 OGL files and into the existing shared Android MSAA helper
- Keep Windows/Linux/macOS host builds intact and avoid changing non-Android rendering behavior

## Plan
1. Map current D1/D2 MSAA state, helper calls, and shared helper coverage
2. Extend `android/app/src/main/cpp/shared/ogl_msaa_android.*` only as needed to preserve existing debug logging and state reset behavior
3. Replace duplicated local `ogl_msaa_destroy_fbo()` and `ogl_msaa_create_fbo()` bodies in D1/D2 with thin wrappers around the shared helper
4. Run scoped format/diagnostics, host build checks, Android native build checks, and at least one MSAA-capable smoke path if available
5. Update the cleanup tranche survey with the implemented extraction and validation results

## Status
- [x] Map current helper state
- [x] Implement shared helper extraction
- [x] Validate builds and smoke checks
- [x] Update cleanup survey

## Result
- Extended `android/app/src/main/cpp/shared/ogl_msaa_android.c` and `.h` so the shared helper owns MSAA FBO create/destroy lifecycle, optional caller logging, and bound-state reset
- Replaced duplicated D1/D2 local FBO lifecycle bodies with thin wrappers around the shared helper while keeping local logging, `ogl_msaa_*` aliases, and `g_msaa_fbo_bound`
- Added MSAA runtime introspection fields: `msaa_samples`, `msaa_max_samples`, and `msaa_fbo_bound`
- Added `android/game_scripts/test_msaa_fbo_smoke_d2.json5` to launch D2 with `MsaaLevel=2`, assert the game sees MSAA, enter gameplay, and exercise the FBO path

## Validation
- `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/ogl_msaa_android.c','android/app/src/main/cpp/shared/ogl_msaa_android.h')` passed
- `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/game_introspect.cpp','android/game_scripts/test_msaa_fbo_smoke_d2.json5')` passed
- `android/run-code-quality.ps1 -Fix -Paths @('android/game_scripts/test_msaa_fbo_smoke_d2.json5')` passed after the D2-specific config fix
- `run-windows-build.ps1 -Target both` passed, with only existing `weapon.c` C4715 warnings
- Gradle Android native builds passed for `:app:buildCMakeDebug[arm64-v8a]` and `:app:buildCMakeDebug[arm64-v8a]-2`
- `android/Run-Emulator.ps1` rebuilt and installed the debug APK, including x86_64 native build for the emulator
- `android/helpers/run_test.ps1 -ScriptName test_msaa_fbo_smoke_d2.json5 -Game d2` passed
- Runtime log confirmed `MSAA FBO create request`, `MSAA FBO created: samples=2 size=640x480`, and later `gr_flip` entries with `msaa=1`
- Focused diagnostics were clean for touched files; focused `git diff --check` had only existing CRLF normalization warnings for `ogl_msaa_android.c` and `.h`
