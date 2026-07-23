# Fix CD batch scalar handling and failure reporting

## Goal

Make the unified CD extraction stage handle single-item PowerShell results and
report every per-disc failure without masking the original diagnostic.

## Plan

- [x] Create the remediation plan
- [x] Read repository instructions and inspect every collection assumption
- [x] Preserve complete failure records without optional-property exceptions
- [x] Normalize remaining scalar-or-array results before using `Count`
- [x] Add focused regression coverage for both failures
- [x] Run scoped code quality, focused tests, build, and CTest

## Validation

- A synthetic single-track extraction retains an array and reports one hash
- A mixed success and missing-source batch reports the original failure and
  exits 1 without an optional `Details` exception
- The equivalent single-result fingerprint normalization was corrected
- The unified-runner and extraction-workflow tests passed
- All 34 CD specs passed structural validation
- `extract_cd` rebuilt and all 13 native CTest suites passed
- Scoped code quality passed; pre-existing style warnings remain in the two
  legacy top-level `game_data` scripts outside the quality wrapper's lint root
