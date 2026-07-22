# Emulator contamination audit

## Plan

- [x] Map the complete automated test catalog to its runners and cleanup paths
- [x] Audit custom emulator tests for inherited process, preference, config, file-set, staged-file, and result-file state
- [x] Audit ordinary JSON automation tests for mutations that escape centralized cleanup
- [x] Fix each concrete contamination gap without changing timeouts
- [x] Run static validation, scoped code quality, and host build verification
- [x] Record audited categories, fixes, and verification

## Findings

- Standalone JSON automation tests share `run_test.ps1`. Their pre-test reset was incomplete, and the runner could leave the app alive after success or an early exit.
- Persistent mod, audio, pending-resume, automation-result, introspection, and quick-record artifacts could influence later tests.
- Custom launcher and double-launch tests did not consistently reset inherited state or stop the app on every exit path.
- Multiplayer and LAN tests cleaned up only on failure or deliberately left apps, lobby services, relays, or logcat readers alive after success.
- SAF redbook fixtures survived some failures. The SAF archiver could exit after moving `descent2.ham`, leaving both the manifest and displaced game data for later tests.
- Extraction tests could leave an active regression file set and mod manifest. Mission ZIP batches and random-preview tests could leave the app running.
- Staged ZIP dependencies without an enabled mod manifest are inert and are intentionally retained. Pilot data remains intentionally persistent except in tests that explicitly require a fresh pilot. Manual and broadcast-helper tests were excluded from automatic teardown changes.

## Verification

- Parsed all changed PowerShell files successfully.
- Scoped `run-code-quality.ps1 -Fix` passed for all 13 changed PowerShell files.
- `test_test_helpers_process_wait.ps1` passed, including new reset coverage.
- `test_validate_automation_catalog.ps1` passed: 63 standalone JSON tests, 18 support scripts, and 45 PowerShell entries.
- Windows host build/test D1 and D2 passed during this work.
- No emulator tests were run, per request.
