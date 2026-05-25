# Game Data Manifest Plan

## Goal
- [x] Add a central JSON manifest for proprietary regression data under `game_data/`
- [x] Add `game_data/manage_data.ps1` with regenerate, verify, and zip export actions
- [x] Reuse existing known hash/test-data conventions where practical
- [x] Validate the script against available fixture or placeholder data without requiring proprietary files

## Notes
- Target data includes GOG installers, Mac demo installers, PC demo installers, and CD image files
- Export should preserve paths relative to the repository root and skip extracted/attendant files
- Manifest generated at `game_data/test_data_manifest.json`
- Script supports interactive menu plus `-Action Regenerate`, `-Action Verify`, and `-Action Export`
- Full local verification passed for 242 files, 13.85 GB
- Zip export smoke-tested with a small temporary manifest to avoid writing a 13.85 GB archive
