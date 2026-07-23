# GOG installer redbook test hardening

## Goal

Harden `test_gog_installer_redbook_unified` against polluted state and nondeterministic launcher/import sequencing without extending timeouts.

## Plan

- [x] Inspect the failing report, complete log, test script, runner, and import-state cleanup behavior
- [x] Identify concurrent setup introspection and preview enumeration entering non-thread-safe HMP/TML conversion
- [x] Serialize native MIDI data operations, report start failure accurately, clear previews during reset, and poll playback state
- [x] Run scoped code quality checks for changed files
- [x] Build and install the debug APK, then run the focused Windows GOG variant to completion
- [x] Run the shared HMP preview regression to check the serialized path independently

## Verification

- `android/run-code-quality.ps1 -Fix -Paths <changed files>`: PASS
- `android/gradlew.bat :app:assembleDebug`: PASS
- `android/tests/test_gog_installer_redbook_unified.ps1 -InstallerVariant d2_windows_exe -SkipPush -TimeoutSeconds 300`: PASS
- `android/helpers/run_test.ps1 -ScriptName test_midi_preview_hmp_unified.json5 -Game d2 -TimeoutSeconds 60`: PASS
