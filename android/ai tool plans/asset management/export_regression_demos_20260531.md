# Export Regression Demos With Test Data

Goal: include input demo regression files in the game data export zip without adding them to the hashed manifest.

## Plan

- [x] Read project instructions and inspect the game data manager export path
- [x] Add a non-manifest export source for `android/regression_demos/`
- [x] Validate that export entries preserve repo-relative paths for root extraction
- [x] Update this plan with the final result
- [x] Include `.dem` and `.rngtrace.jsonl` regression demo companion files in the export-only set
- [x] Revalidate the expanded archive entry set

## Result

- `game_data/manage_data.ps1 -Action Export` now appends `.dximdemo`, `.dem`, and `.rngtrace.jsonl` files from `android/regression_demos/` after the hashed manifest entries
- Regression demo files are written with repo-relative archive names like `android/regression_demos/name.dximdemo`
- Manifest generation, listing, checking, and hashing remain limited to the existing tracked test data set
- Validation used a temporary empty manifest and confirmed 33 regression demo archive entries with the expected paths: 11 `.dem`, 11 `.dximdemo`, and 11 `.rngtrace.jsonl`
