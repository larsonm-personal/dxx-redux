# Level statistics metadata header

## Goal

Add per-level geometry and content statistics to the metadata model and display them as a header in the level metadata view, using the Legacy of Chaos README figures as a validation reference.

## Investigation

- [x] Locate the supplied Legacy of Chaos README and confirm the exact reference figures
  - The supplied `temp/mission/...` path is absent, but the same archive is checked in as `game_data/mission_files/legacy.zip`
  - `legacy.txt` contains a statistic line for every level, not only the quoted level 2 line
  - Level 1: 388 cubes, 133 walls, 14 triggers, 189 objects, 52 textures
  - Level 2: 518 cubes, 115 walls, 16 triggers, 229 objects, 53 textures
  - Level 3: 785 cubes, 240 walls, 21 triggers, 289 objects, 44 textures
- [x] Trace native level analysis, JSON serialization, Kotlin parsing, persisted regression JSON, and the metadata UI
  - Android analysis serializes loaded levels in `jni_level_metadata.cpp`
  - Host regeneration has a second serializer in `headless_metadata_dump_main.cpp`
  - `LevelMetadata.kt` parses both into `LevelMetadataLevelRow`
  - `LauncherScriptExecutor.kt` writes normalized Android regression output
  - `regenerate_all_mission_metadata_host.ps1` converts host fields into the checked-in JSON shape
  - `SetupSections.kt` displays the per-level details in `LevelMetadataLevelDialog`
- [x] Map cubes, walls, triggers, objects, and textures to authoritative engine data
  - Cubes: `Num_segments`; the engine calls these segments, but the UI should retain the historical word `cubes`
  - Walls: `Num_walls`
  - Triggers: `Num_triggers`, matching the editor's wall-trigger statistic; D2X object triggers are not part of this legacy figure
  - Objects: `num_objects`, which is the loaded level's live object count and includes all object types
  - Textures: distinct base and overlay wall texture IDs used on rendered sides
- [x] Verify the historical texture-count semantics against the editor lineage
  - The current DLE source is based on DMB2 and its `CDiagTool::CountTextures` implementation supplies the exact algorithm
  - Visit every side whose child is `-1` or whose wall index is valid
  - Count each base texture ID once
  - Strip the orientation bits from `tmap_num2`, ignore overlay ID zero, and count each remaining overlay ID once
  - Do not count textures on an internal portal that has no wall
- [x] Compare existing analyzer data with the README where possible
  - `game_data/mission_files/legacy.json` confirms the expected three files and level names, including `trident.rl2` for the quoted level 2 statistic
  - Existing metadata does not serialize any of the five raw totals, so an exact calculated comparison belongs in the implementation validation phase
- [x] Define implementation, cache/versioning, UI, and test changes

## Implementation plan

### Phase 1: Central native statistic calculation

- [x] Add a small shared Android-side native helper for a `level_statistics` value containing segment, wall, wall-trigger, object, and used-texture counts
- [x] Keep the D1 and D2 engine globals as the source of truth and avoid edits in `d1/` and `d2/`
- [x] Implement the texture count once in the shared helper using the DLE semantics above, with bounds checks for segment, wall, and texture IDs
- [x] Call the helper only after `load_level` has succeeded, before any future operation could create transient gameplay objects
- [x] Serialize `segment_count`, `wall_count`, `trigger_count`, `object_count`, and `texture_count` in both `jni_level_metadata.cpp` and `headless_metadata_dump_main.cpp`
- [x] Emit zero placeholders in failed native rows for a stable JSON shape, but do not display a statistic header for failed levels

### Phase 2: Kotlin model, cache, and normalized JSON

- [x] Extend `LevelMetadataLevelRow` and `LevelMetadataResult.fromJson` with the five integer fields
- [x] Change `LEVEL_METADATA_RESULT_CACHE_SCHEMA` from v1 to v2 so existing successful result caches cannot silently parse missing statistics as zeros
- [x] Do not increment `ROUTE_METADATA_CACHE_GENERATION`; these fields do not alter route calculations, and the result-cache schema already provides the needed invalidation
- [x] Add the five fields to `LauncherScriptExecutor.levelMetadataResultJson`
- [x] Add the host raw-to-checked-in mappings to `ConvertTo-CheckedInLevelJson` in `regenerate_all_mission_metadata_host.ps1`
- [x] Preserve the existing normalized field order in checked-in mission JSON

### Phase 3: Per-level header UI

- [x] Add a pure formatter that produces the historical sentence shape, for example `Statistics: 518 cubes, 115 walls, 16 triggers, 229 objects, 53 textures`
- [x] Render that sentence directly below the level name in the title/header area of `LevelMetadataLevelDialog`
- [x] Let the sentence wrap on narrow screens and use the secondary text color and a smaller font than the level name
- [x] Keep the main mission table unchanged; adding five more columns would make its already-wide layout substantially harder to scan

### Phase 4: Automated validation and regression data

- [x] Add a native unit test for used-texture counting that covers duplicate base/overlay IDs, overlay orientation bits, overlay zero, internal portals without walls, and connected sides with walls
- [x] Extend `LevelMetadataResultTest` to verify all five JSON fields and the exact formatted header
- [x] Extend `LevelMetadataResultCacheTest` to prove a v1/missing-stat cache cannot be reused after the schema change
- [x] Regenerate focused host metadata for `legacy.zip` first and compare all three levels to the README, requiring exact matches unless an engine load transformation is identified and documented
- [x] Add a high-level regression test over `game_data/mission_files/legacy.json` with these expected calculated tuples, while separately retaining the README texture reference:
  - Level 1: `388, 133, 14, 189, 52`
  - Level 2: `518, 115, 16, 229, 54` (README: 53)
  - Level 3: `785, 240, 21, 289, 44`
- [x] Run the zero-parameter host metadata regeneration so every checked-in mission receives the new fields
- [x] Run the focused native tests, Android JVM tests, mission JSON normalization tests, and mission metadata regression tests
- [x] Run scoped code quality over the changed C/C++, Kotlin, PowerShell, CMake, and test files
- [x] Run the Windows D1 and D2 build verification required by the repository instructions

## Results

- The three Legacy levels match the README exactly for cubes, walls, triggers, and objects.
- Texture counts are 52, 54, and 44. Level 2 is one above the README because texture ID zero is present on a rendered side in the loaded engine data and is counted as a valid base texture by the editor-derived algorithm. The regression requires the calculated tuple exactly and permits at most a one-texture difference from the README reference.
- Full host regeneration completed with 115 missions: 114 passed, 1 skipped, and 0 failed.
- The standalone native statistic test, full Android JVM suite, Android debug build, mission metadata regressions, JSON normalization, scoped code quality, and Windows D1/D2 builds all pass.

## Expected touched areas

- `android/app/src/main/cpp/shared/`: shared statistic collector
- `android/app/src/main/cpp/jni_level_metadata.cpp`: Android result serialization
- `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`: host result serialization
- `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt`: Kotlin row and JSON parsing
- `android/app/src/main/java/com/dxxredux/app/LevelMetadataResultCache.kt`: result-cache schema invalidation
- `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt`: normalized Android regression JSON
- `android/app/src/main/java/com/dxxredux/app/SetupSections.kt`: per-level header
- `android/helpers/regenerate_all_mission_metadata_host.ps1`: normalized host regression JSON
- Native, Kotlin, and PowerShell tests plus regenerated `game_data/mission_files/*.json`

## Implementation notes

- `LevelMetadata.kt` and nearby route-precompute files currently contain unrelated user changes. Implementation must preserve those changes and limit edits in `LevelMetadata.kt` to the row model and parser sections.
- The downloaded DLE source used for this investigation is temporary evidence under `temp/level_statistics_investigation` and is not part of the proposed product change.
