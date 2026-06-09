# JSON baseline pretty print

## Goal
Normalize and pretty-print generated regression JSON baselines so they are practical to review and commit.

## Plan
- [x] Identify mission metadata JSON outputs and the secret-area regression JSON fixture.
- [x] Pretty-print the selected JSON files with stable deterministic formatting.
- [x] Validate the formatted files still parse as JSON.
- [x] Run scoped code-quality/BOM checks.

## Notes
- Formatted `game_data/mission_files/descent_maximum_fixed.json`.
- Formatted `game_data/mission_files/Obsidian.json`.
- Formatted `game_data/mission_files/plutonia.json`.
- Normalized `android/test_fixtures/secret_area_base_game_baseline.json`.
- Verified all four files parse as JSON after formatting.
- Ran `android/run-code-quality.ps1 -Fix` on the formatted JSON files and this plan.
