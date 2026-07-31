# Combined extraction launch fixture

## Goal

Add a small declarative fixture that combines independently extracted CD
regression sources, then use it to verify that the standalone Vertigo expansion
launches when paired with a complete Descent II disc.

## Plan

- [x] Inspect extraction spec generation, discovery, validation, and launch flow
- [x] Add `combined_launch.json5` generation with validated component spec paths
- [x] Teach the extraction runner to merge component `data_tracks` directories
- [x] Add focused generator and composition tests
- [x] Generate and run the Descent II plus Vertigo launch regression
- [x] Review the final diff and record validation results

## Result

The helper combines the Descent II v1.1 and Vertigo extraction specs, places
the Vertigo descriptor and HOG under `missions/`, and generates a seven-file
launch oracle. The emulator regression explicitly selected
`Descent 2: Vertigo` and reached `deep kraeg tunnel system`.

Validation completed:

- `android/tests/test_generate_regression_specs.ps1`
- `android/tests/test_extract_regression_workflow.ps1`
- `android/tests/test_validate_extract_regression_specs.ps1`
- Focused `android/run-code-quality.ps1 -Fix`
- Full combined launch through `android/tests/test_all_extracts.ps1`
- Stable forced regeneration preserved the passing regression byte-for-byte
