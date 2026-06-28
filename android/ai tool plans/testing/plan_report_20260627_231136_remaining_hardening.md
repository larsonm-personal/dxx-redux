Report 20260627 231136 Remaining Hardening - 2026-06-28

Goal:
- Harden the remaining two failures from report_20260627_231136.md without weakening test assertions or extending timeouts as the primary fix.

Targets:
- test_saf_archiver
- test_gog_installer_redbook_unified

Plan:
- [done] Inspect full logs and the related PS1/JSON5 scripts.
- [done] Identify whether each failure is stale setup state, launch synchronization, or product behavior.
- [done] Apply focused robustness fixes that preserve the purpose of each test.
- [done] Build and install the debug APK if source changes require it.
- [done] Run scoped code quality on touched files.
- [done] Run both focused tests.
- [done] Record verification and residual risk.

Notes:
- test_saf_archiver had a private logcat-only result watcher, unlike run_test.ps1. It now clears and watches automation_result.json/automation_log.jsonl first, with logcat only as fallback.
- test_gog_installer_redbook_unified verified imported audio, but did not pin game-side music preferences before launch. It now writes CD music prefs after import/preview checks.
- No APK rebuild was needed for this tranche because only PS1/JSON5 automation scripts changed.

Verification:
- PASS: .\android\tests\test_saf_archiver.ps1 -NoBuild
- PASS: .\android\tests\test_gog_installer_redbook_unified.ps1 -TimeoutSeconds 300
- PASS: .\android\run-code-quality.ps1 -Fix -Paths android\tests\test_saf_archiver.ps1 android\game_scripts\test_gog_installer_redbook_unified.json5

Residual risk:
- The GOG test run's durable result reported PASS at 49/48 steps after the handoff; this appears to be the existing launcher/game handoff accounting rather than a failure.
