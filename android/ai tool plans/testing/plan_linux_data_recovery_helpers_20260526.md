# Linux Data Recovery Helpers Plan

## Goal
- [x] Detect missing CD extraction oracles before CD-requiring tests fail deep in execution
- [x] Offer a clear regeneration path for missing CD oracles
- [x] Recover or infer valid D1/D2 host replay data directories from populated extracted data
- [x] Validate the helper behavior with focused script checks

## Notes
- Keep fixes in Android test scripts and shared helpers
- Avoid changing base game source
- Prefer existing extraction and data indexing conventions over new storage layouts
- Added SHA256 index scanning for extracted CD image data so standard game data dependencies can resolve copied/regenerated data
- Focused PowerShell parser checks passed
- `validate_extract_regression_specs.ps1` and `test_validate_extract_regression_specs.ps1` both validated 34 CD regression specs
- `run_input_demo_replay.ps1 -ResolveDataDirOnly` resolved both default copied data and an explicit CD-image `data_tracks` directory
