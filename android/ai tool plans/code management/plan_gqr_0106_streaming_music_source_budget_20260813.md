# GQR-0106 streaming mission-music source budget plan

## Goal

Preserve exact source sizes and one nested expansion budget while cataloging or materializing music from streaming ZIP/DXA and HOG containers

## Constraints

- Keep all product changes in branch-added Android files
- Reuse `ExtractionBudget` and `MissionMusicCatalogBudget`
- Preserve data-descriptor entry sizes discovered during streaming
- Treat zero or negative compressed metadata conservatively so ratio accounting cannot be disabled
- Thread one budget through nested ZIP/DXA and HOG work
- Bound MIDI materialization and honor cancellation
- Do not run Gradle until the root agent grants the serialized slot

## Work

- [x] Audit streaming ZIP/DXA, HOG, catalog, staging, and MIDI paths
- [x] Implement exact source-size and cumulative nested-budget propagation
- [x] Add descriptor/unknown-size, expansion, nesting, MIDI, cancellation, and ordinary regression tests
- [x] Run scoped formatting and static checks
- [x] Run focused Gradle tests after the serialized slot is granted
- [x] Report changed paths, diff metrics, validation, and remaining risks

## Results

Streaming DXA entries are now consumed before their data descriptors are trusted, so catalog tracks retain exact expanded sizes and unusable compressed metadata cannot bypass final ratio validation. One attempt-owned `ExtractionBudget` now covers catalog DXA/HOG expansion, compressed-audio staging, nested DXA-to-HOG work, and bounded MIDI reads. MIDI byte materialization is capped at 64 MiB and all extraction-budget operations honor thread interruption

Product changes are limited to branch-added Android Kotlin in `ExtractionLimits.kt`, `MissionMusicCatalogBudget.kt`, `MissionZipMusic.kt`, and `MissionZipMusicStageManager.kt`. Tests were extended in `ExtractionLimitsTest.kt`, `MissionMusicCatalogBudgetTest.kt`, and `MissionZipMusicStageManagerTest.kt`. No `d1/` or `d2/` file changed

Validation passed:

- Focused Gradle command covering all three suites completed successfully in 8 seconds after the final warning cleanup
- `ExtractionLimitsTest`: 13 tests, zero failures or errors
- `MissionMusicCatalogBudgetTest`: 11 tests, zero failures or errors
- `MissionZipMusicStageManagerTest`: 15 tests, zero failures or errors
- Scoped code-quality formatting and lint passed for all owned paths
- Repository `git diff --check` passed

Coverage includes real streaming data descriptors, exact and one-over cumulative expansion output, expansion-ratio rejection, zero and negative compressed metadata, shared nested DXA/HOG budgets, oversized MIDI rejection before reading, cancellation, sequential attempts, and ordinary top-level/nested staging and MIDI regressions
