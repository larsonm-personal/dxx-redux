# Investigate generated regression changes

## Goal

Explain the changes in the Descent Anniversary fingerprint manifest and Dimensions for Descent mission metadata, including whether each change is expected or indicates a generator regression.

## Plan

- [completed] Inspect the working-tree diffs and repository history for both generated files
- [completed] Identify the exact generators and source inputs responsible for each changed field
- [completed] Correlate the changes with recent regeneration reports and code changes
- [completed] Run focused read-only validation where useful and report conclusions

## Findings

- The Anniversary fingerprint change is expected. The old digest predates the MODE1/2048 sector-stride correction, while the new digest matches the maintained disc database and extraction manifest
- The Dimensions metadata change is not a real metadata change. A targeted run selected Dimensions without the earlier Levels of the World source, bypassing cross-source descriptor deduplication and adding seven duplicate missions
- All 48 pre-existing Dimensions missions are byte-equivalent as JSON after ignoring shifted target indexes
- The targeted metadata sampler must preserve canonical cross-source deduplication before its sampled write set is accepted

## Follow-up fix

- [completed] Preserve canonical descriptor ownership when filtering sampled CD metadata sources
- [completed] Add regression coverage for a later sampled source that duplicates an earlier unselected source
- [completed] Restore and verify the Dimensions metadata oracle
- [completed] Run focused tests and scoped code quality

## Follow-up validation

- A sampled Dimensions-only host regeneration passed in 29 seconds and reproduced the committed 48-mission oracle byte-for-byte
- CD source, metadata normalization, travel-time, and master regeneration tests passed
- Scoped code quality passed
- Windows D1 and D2 builds passed
- CTest passed 33/33 D1 tests and 40/40 D2 tests
