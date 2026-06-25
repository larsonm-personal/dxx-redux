Quick Record Sidecar Hardening - 2026-06-24

Goal:
- Harden one remaining failure from report_20260623_230322.md without weakening the feature coverage or papering over timing with larger timeouts.

Target:
- test_quick_record_classic_sidecar_stage, with test_quick_record_classic_sidecar_install checked if the stage artifact changes affect it.

Plan:
- [done] Inspect the failing report logs, scripts, and automation support for quick-record sidecar staging.
- [done] Identify the semantic condition the test should require instead of a brittle absolute recorded-frame count.
- [done] Patch the smallest script or harness area needed to make the test fault tolerant while preserving coverage.
- [done] Run the focused stage test and, if relevant, the paired install test.
- [done] Record results and any residual risk.

Notes:
- The failed stage run reached gameplay and quick recording, but only observed input_demo.frame_count=5 before the old >=9 condition timed out.
- The downstream install failure was caused by the stage test stopping before it produced a staged demo.
- Setup introspection already exposes staged_input_demos[].frame_count, has_rng_trace, has_classic_demo, and header_readable, so the better contract is to require nonempty live recording and then verify the staged artifact/header/sidecars.
- Updated test_quick_record_classic_sidecar_stage.json5 to require live recording frame_count >= 1, then require staged game=d2, level=1, frame_count >= 1, header_readable=true, has_rng_trace=true, and has_classic_demo=true.
- Focused run passed: .\android\helpers\run_test.ps1 -ScriptName test_quick_record_classic_sidecar_stage.json5 -Game d2, output temp\test_quick_record_stage_hardened_20260624.txt.
- Paired downstream run passed: .\android\helpers\run_test.ps1 -ScriptName test_quick_record_classic_sidecar_install.json5 -Game d2, output temp\test_quick_record_install_after_stage_hardened_20260624.txt.
- Scoped code quality passed for android\game_scripts\test_quick_record_classic_sidecar_stage.json5.
