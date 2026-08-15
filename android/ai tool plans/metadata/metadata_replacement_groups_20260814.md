# Metadata Replacement Groups

## Goal

Expand metadata replacement analysis into four hierarchical viewer groups: ship stats, weapon balance, robot changes, and texture/model/sound replacements.

## Plan

- [x] Define a hierarchical replacement JSON/model shape that preserves exact base and mod values.
- [x] Capture base engine data before mission HAM/HXM analysis and compare the analyzed mod state without launching gameplay.
- [x] Populate ship, weapon, robot, and asset replacement groups with concise summaries and changed subrows.
- [x] Replace the flat viewer table with collapsed top-level groups and collapsed per-weapon/per-robot summaries.
- [x] Add native-data, parser, hierarchy, and unchanged-mod regression coverage.
- [x] Run scoped formatting, native tests, focused Android tests, real-mod analysis, and D1/D2/Android builds.

## Boundaries

- Analysis must not require starting or playing a mission.
- Only changed or added data should appear; unchanged groups remain hidden.
- Keep comparisons based on engine-loaded data instead of reimplementing HAM/HXM parsing in Kotlin.
- Avoid presenting hard-coded limits such as ammunition capacity as mod replacements.

## Result

- Metadata analysis snapshots the base D2 ship, weapon, robot, and polygon-model definitions before loading mission data.
- Changed categories appear as collapsed viewer rows; player ship, weapon, and robot entries are independently collapsed beneath them.
- Ship fields, selected weapon and robot balance fields, HXM additions, changed polygon models, level POG texture counts, and replacement sound banks are reported with base/mod values where applicable.
- Unchanged categories and stock missions do not display replacement rows.
- Obsidian level 4 reports one player-ship change, 36 robot entries, changed polygon models, and replaced textures.
- D1/D2 Windows builds, 73 native host tests, focused Android tests, scoped quality checks, and the multi-ABI debug APK build pass.
