# Objective Numbering Unification

## Goal

Use the metadata route index as the single objective number everywhere: start is index 0, and actual objectives are numbered 1 through N. The metadata viewer leaves start unnumbered, while the automap next-objectives list and in-world markers both display the same 1 through N numbers.

## Plan

- [x] Trace numbering in the metadata viewer, automap objective list, and in-world automap markers.
- [x] Centralize or consistently derive display numbers from the full metadata route index.
- [x] Leave the metadata viewer start row unnumbered and number only actual objectives 1 through N.
- [x] Make filtered next-objective entries retain their full-route objective numbers.
- [x] Make in-world objective markers use those same full-route objective numbers.
- [x] Add focused integration coverage for start suppression and matching automap numbers.
- [x] Run scoped checks, Windows D1/D2 builds, and the relevant Android integration test.

## Validation

- `:app:testDebugUnitTest --tests com.dxxredux.app.LevelMetadataRouteNumberingTest`
- Scoped Kotlin and C/C++ code-quality checks
- `run-windows-build.ps1 -Target both`
- `:app:assembleDebug` for arm64-v8a, armeabi-v7a, and x86_64
- `test_obsidian_level1_objective_markers.json5`
- `test_obsidian_level1_next_objectives.json5`

## Notes

- Preserve the existing Obsidian level 1 and level 2 fixes already present in the worktree.
- Avoid renumbering a filtered list by its visible position; numbers must remain stable as objectives complete.
