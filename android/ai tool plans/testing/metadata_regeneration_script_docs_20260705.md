# Metadata Regeneration Script Docs 2026-07-05

Goal: document the mission metadata regeneration script choices in `.github/copilot-instructions.md`.

Plan:
- [done] Add clear guidance for the configurable emulator batch script
- [done] Add clear guidance for the zero-parameter emulator wrapper
- [done] Add clear guidance for the no-emulator host regeneration path
- [done] Re-run scoped quality checks after host helper implementation

Notes:
- Documented `android\helpers\run_mission_zip_batch.ps1` as the configurable emulator/device batch path.
- Documented `android\helpers\regenerate_all_mission_metadata.ps1` as the current zero-parameter full regeneration helper.
- Updated `android\helpers\regenerate_all_mission_metadata_host.ps1` guidance to describe the implemented zero-parameter host helper.
- Previous validation: scoped code quality passed for `.github\copilot-instructions.md`; direct `git diff --check` and UTF-8 BOM checks passed for the instructions and this plan file.
- Current validation: touched-file BOM checks and `git diff --check` passed after the host helper documentation update.
