# Robot Names and Preview Plan

## Goal

Add editable D1/D2 robot-name JSON tables to the metadata replacement viewer and define the implementation plan for an engine-rendered robot preview launched from that viewer.

## Work Plan

- [x] Confirm base D1/D2 robot ID ranges and choose an asset schema.
- [x] Add complete blank-name JSON arrays for both games.
- [x] Load and validate the tables in the launcher with `Robot N` fallback behavior.
- [x] Apply populated names to robot entries in the metadata replacement viewer.
- [x] Add focused JSON loading, lookup, and fallback tests.
- [x] Write the detailed robot-preview implementation plan.
- [x] Run scoped quality checks and focused Android tests/build.

## Result

- Added D1 entries 0 through 29 and D2 entries 0 through 65 as editable JSON assets.
- Robot replacement JSON now carries a numeric `number` field instead of requiring label parsing.
- Filled names display as `Name (Robot N)`; blank and mod-added entries retain the analyzer's `Robot N` label.
- The catalog enforces a complete, ordered stock range so hand-edited table mistakes fail tests instead of shifting names onto the wrong robots.
- Added a phased robot-preview plan covering provenance, staging, process lifecycle, native rendering, input, base/mod comparison, introspection, and tests.
- Focused catalog and metadata parser tests, scoped quality checks, the D2 Windows build, real Obsidian output, and the multi-ABI debug APK build pass.
