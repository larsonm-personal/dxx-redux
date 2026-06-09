# Level metadata view study

## Goal
- Study feasibility for a unified Android launcher dialog that shows per-level metadata for archives and mission files
- Include level number, level name, robots, hostages, secrets, matcens, and energy centers
- Decide which file types should expose a "Level metadata" preview button

## Plan
- [x] Inspect existing launcher file metadata previews
- [x] Inspect mission ZIP and constituent scan flow
- [x] Inspect native/headless level analysis surfaces and available counts
- [x] Define feasible file-type coverage and implementation approach
- [x] Note risks, test strategy, and follow-up implementation phases

## Findings
- Feasible, but the per-level table should be produced through the engine/native loader, not by duplicating RDL/RL2/HOG parsing in Kotlin.
- Current launcher previews are content summaries only:
  - `GameFileMetadata.summarizeLocalFile` handles local HOG/DXA/PIG/POG.
  - `GameFileMetadata.summarizeZipConstituent` handles ZIP child HOG/DXA/PIG/POG and MSN/MN2 descriptors.
  - `GameFileFormats.isMetadataInspectable` currently gates only HOG/DXA/PIG/POG.
- Mission descriptors are already parsed in Kotlin:
  - `GameFileFormats.parseMissionDescriptor` reads normal levels, secret levels, secret origins, and referenced assets.
  - `MissionZip.inspect` finds ZIP constituents and returns the selected mission descriptor plus constituent list.
- The current headless secret-area dump already loads levels and serializes:
  - level number, name, and level file
  - secret count
  - energy center count plus raw/segment/debug distances
  - matcen count plus raw/segment counts
- Robot and hostage counts should be added in the same native post-load pass by scanning live objects:
  - count `OBJ_ROBOT`
  - count `OBJ_HOSTAGE`
  - skip objects flagged `OF_SHOULD_BE_DEAD`

## Base Game Level Counts
- The "30 levels" HOG preview is currently a total level-file count, not 30 normal levels plus secrets.
- Current baseline data shows:
  - D1 First Strike: 30 total, 27 normal, 3 secret
  - D2 Counterstrike: 30 total, 24 normal, 6 secret
- The preview should present this as normal plus secret counts when it can infer or load mission ordering.

## Button Coverage
- Direct file previews should expose "Level metadata" for:
  - `.hog` when it contains level files or is recognized as a base/mission HOG
  - `.msn` and `.mn2` when referenced level files/HOGs can be resolved from the same data set
  - `.rdl`, `.rl2`, `.sdl`, `.sl2` as single-level analysis
  - `.zip` when `MissionZip.inspect` succeeds, or when constituents include level-containing files
- Mission ZIP top-level details should expose the button when a constituent is a descriptor, HOG, DXA, or direct level file.
- Mission ZIP constituent dialogs should expose the button for `.hog`, `.msn`, `.mn2`, `.rdl`, `.rl2`, `.sdl`, `.sl2`, and maybe `.dxa` when the DXA contains direct level sources.
- `.dxa` support should be conservative at first:
  - show the button only when the archive contains direct level/HOG/descriptor files
  - do not try to fully model patch-overlay behavior in the first pass

## Recommended Design
- Add one launcher-level data model for analysis results, for example:
  - source title/path
  - game hint
  - problems
  - rows with level number, secret flag, level name, level file, robots, hostages, secrets, matcens, energy centers
- Add a single `LevelMetadataDialog` composable used by all entry points.
  - Open immediately in a loading state.
  - Run analysis on `Dispatchers.IO`.
  - Cache results by file path, size, modified time, and optional ZIP entry path.
  - Use one compact table with columns: Level, Name, Robots, Hostages, Secrets, Matcens, Energy.
- Add a native analysis bridge rather than Kotlin binary parsers.
  - Extract or share the headless dump's runtime/level loop into a reusable analyzer.
  - Keep `secret_area_scan.c` focused on secret scanning and keep metadata counting in `level_metadata_scan.c`.
  - Return JSON or a small structured JNI result array.
- For ZIP packages, stage needed constituents to an app cache analysis directory, then invoke the same native analyzer on staged files.
- For HOG-only missions without a descriptor, use a best-effort fallback:
  - enumerate level entries and sort by conventional names
  - mark secret levels by `.sdl`/`.sl2` extension
  - prefer mission descriptor ordering whenever available

## Risks And Open Points
- Custom missions can rely on extra HAM/HXM/PIG data. Object counts will usually be load-level facts, but full compatibility needs the analyzer to mount the same support files the game would mount for launch.
- SAF leave-in-place files may need a temporary copy because native analysis wants filesystem paths.
- HOG-only ordering is inherently weaker than descriptor-backed ordering.
- DXA patch archives can modify an existing mission rather than directly contain one, so broad DXA analysis should wait until a patch-aware path exists.
- The table could be slow on large mission packs, so async execution and caching are important.

## Suggested Implementation Phases
- Phase 1: Native analyzer API for base/local HOG and single level files, including robots and hostages.
- Phase 2: Kotlin `LevelMetadataDialog` and button wiring for direct local HOG/RDL/RL2/SDL/SL2.
- Phase 3: Descriptor-backed MSN/MN2 resolution and corrected normal/secret preview counts.
- Phase 4: Mission ZIP top-level and constituent staging support.
- Phase 5: Conservative DXA support and regression tests with a small fixture mission ZIP.
