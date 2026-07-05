# Manual Metadata Regeneration Progress

## Goal

Document the command for regenerating mission metadata JSON files, verify whether it reports useful progress during long runs, and improve progress output if needed.

## Plan

1. Done: Inspect the existing mission metadata batch scripts and previous regeneration notes.
2. Done: Identify the canonical manual command for full metadata regeneration.
3. Done: Add progress reporting to the regeneration script if the current output is not clear enough for a long manual run.
4. Done: Add a note to `.github/copilot-instructions.md` with the command and expected behavior.
5. Done: Run focused validation for changed scripts and documentation.

## Notes

- Manual full regeneration should use `android\helpers\run_mission_zip_batch.ps1 -MetadataOnly` from the repo root.
- Leave regression JSON enabled by omitting `-NoRegressionJson`; that updates the `.json` files next to the mission archives.
- The batch runner now reports the output directory, regression JSON mode, `[current/total]` start and completion lines, elapsed time, and pass/skip/fail counts.
