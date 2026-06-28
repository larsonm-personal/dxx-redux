Report 20260627 231136 Hardening - 2026-06-28

Goal:
- Inspect report_20260627_231136.md, pick two failures with durable hardening opportunities, and avoid timeout-only or assertion-removal fixes.

Selected Targets:
- test_saf_redbook / test_gog_installer_redbook_unified: same skip_briefing failure shape after redbook/import launch.
- test_merged_wall_two_pass_debug_mode_probe: merged wall snapshot face-order assertion differs from expected debug-mode face.

Plan:
- [completed] Read full failure logs and the related scripts/helpers.
- [completed] Identify whether each failure is brittle automation/test contract or real product logic.
- [completed] Apply focused hardening changes that preserve the test purpose.
- [completed] Run scoped validation for changed tests/helpers.
- [completed] Run formatting/lint checks for touched files.
- [completed] Record verification and residual risk.
