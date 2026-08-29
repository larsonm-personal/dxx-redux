# Metadata progress bar order

## Goal

Place the stable `Estimated level progress` bar second in metadata calculation displays, ahead of the frequently resetting current-task bar, while keeping all views consistent.

## Plan

- [x] Locate every shared and view-specific rendering site for analysis progress
- [x] Reorder the rendered rows without changing progress calculation or labels
- [x] Add or update focused UI/model coverage if an existing test seam supports ordering
- [x] Run scoped quality checks, tests, and the Android debug build

## Findings

- The metadata calculating dialog renders all three rows through `LevelMetadataAnalysisProgressView`.
- The Advanced precompute view is separate. It has one overall mission bar followed by textual current-level and task details, so it does not contain the same second and third bars to swap.
- The rendering change is a direct swap of the existing nullable `estimatedLevel` and `currentLevel` blocks. Progress calculation, labels, and null behavior are unchanged, so no additional model seam or test is warranted.

## Validation

- Scoped Kotlin formatting and repository quality checks passed.
- `:app:testDebugUnitTest` passed.
- `:app:assembleDebug` passed with JDK 21 for arm64-v8a, armeabi-v7a, and x86_64.
