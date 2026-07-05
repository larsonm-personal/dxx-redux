# Host Mission Metadata Regeneration Design 2026-07-05

Goal: design a fast metadata regeneration path that runs the same Kotlin/Java and native C/C++ analysis code without Android emulator involvement.

Plan:
- [done] Trace the current emulator-based mission metadata batch flow and identify the exact shared/native boundaries
- [done] Identify what must be reused verbatim versus what can be hosted by a JVM or command-line runner
- [done] Propose a build/run architecture that can process all mission packs in under a minute
- [done] List implementation phases, test coverage, and risks before code work begins

Current flow:
- `regenerate_all_mission_metadata.ps1` builds the Android APK, installs it, then calls `run_mission_zip_batch.ps1`
- `run_mission_zip_batch.ps1` pushes each archive and a launcher automation script into app-private storage
- `LauncherScriptExecutor` imports the mission ZIP, builds `LevelMetadataTarget` objects, calls `LevelMetadataAnalyzer`, then writes the checked-in metadata JSON shape
- `LevelMetadataAnalyzer` stages requested archive entries and starts a D1 or D2 Android service
- `LevelMetadataAnalysisService` calls `LevelMetadataNativeBridge`, which loads `dxx-redux-d1` or `dxx-redux-d2`
- `jni_level_metadata.cpp` initializes the game runtime, loads missions/levels, calls the shared scanner through `secret_area_rescan_current_level`, and returns `dxx-level-metadata-v1`

Useful existing host pieces:
- `dxx-redux-d1-headless-metadata` and `dxx-redux-d2-headless-metadata` already build in the native desktop trees
- These host executables already initialize the game without Android and serialize route steps, guidebot fields, boss/reactor fields, travel data, and secret-area data
- Their current schema is `dxx-secret-area-baseline-v1`, not the launcher `dxx-level-metadata-v1` schema
- Probe results on this machine: D2 base mission host scan took about 1.3 seconds, D1 base mission host scan took about 0.8 seconds

Design decision:
- Reuse the existing headless native runtime instead of creating a separate simulator
- Extract the Android JNI metadata analyzer into a shared native core so Android JNI and host executable use the same C++ analyzer and serializer
- Extract the pure Kotlin target discovery and checked-in JSON projection so the emulator path and host path use the same JVM code
- Keep the current emulator helper as the end-to-end phone test, but make the host path the normal regeneration path

Proposed architecture:

1. Shared Kotlin/JVM metadata core
- Move or source-share the pure pieces used by metadata generation into a JVM-safe module or source set:
  - `GameFileFormats`
  - `GameFileMetadata`
  - `MissionZip`
  - `ArchiveFiles` interface and data model
  - `LevelMetadataTarget`
  - `LevelMetadataTargets`
  - request building/staging from `LevelMetadataAnalyzer`
  - checked-in metadata projection from `LauncherScriptExecutor.levelMetadataResultJson`
- Leave Android-only orchestration in the app:
  - `Context`
  - services
  - process polling
  - crash log collection
  - launcher automation
- Add a small host-only archive adapter:
  - ZIP through `java.util.zip`
  - 7z through Apache Commons Compress where possible
  - optional host `7za` fallback for troublesome archives
  - no dependency on `sevenzipjbinding-4Android` in the host module

2. Shared native analyzer core
- Split `jni_level_metadata.cpp` into:
  - a platform-neutral analyzer file, for example `level_metadata_native_analyzer.cpp`
  - a JNI wrapper that only converts `jstring` in and out
  - a host wrapper that reads requests from disk/stdin and writes JSON
- The core API should look roughly like:
  - `std::string level_metadata_analyze_request_json(const char *request_json, level_metadata_platform *platform)`
- The request schema should stay `dxx-level-metadata-request-v1`
- Android and host should both emit raw `dxx-level-metadata-v1`, then Kotlin should project it into the checked-in mission metadata JSON shape

3. Host native batch mode
- Add request mode to the headless metadata executables:
  - single request: `-levelmeta-request <request.json> -levelmeta-json-out <result.json>`
  - batch request: `-levelmeta-request-jsonl <requests.jsonl> -levelmeta-output-dir <dir>`
- Keep one D1 process and one D2 process alive for a batch so native runtime initialization happens once per game
- Each request should mount only its staged dir and HOG paths, run analysis, write result, then unmount request-specific search paths
- If PHYSFS or mission globals prove unsafe to reuse, fall back temporarily to one process per archive, but measure it before accepting the slower path

4. Host CLI
- Add a Gradle JVM application module, for example `:mission-metadata-host`
- The CLI should:
  - find the repo root and `game_data/mission_files`
  - resolve D1 and D2 base data dirs using the same paths as existing host tests
  - enumerate `.zip` and `.7z` mission archives in deterministic order
  - inspect each archive with shared Kotlin target discovery
  - stage only the files needed by each target
  - write D1 and D2 request JSONL files
  - invoke the D1 and D2 native host analyzers
  - parse raw results with `LevelMetadataResult.fromJson`
  - apply the shared checked-in JSON projection
  - normalize JSON before writing to `game_data/mission_files/<archive-stem>.json`
  - write a summary JSON and failed archive list under `android/temp/mission_zip_host_metadata/<timestamp>`
- Progress should show `[current/total]`, game, archive name, elapsed time, pass/skip/fail counts, and output path

5. Zero-parameter helper
- Add `android/helpers/regenerate_all_mission_metadata_host.ps1`
- It should have no `param` block
- It should:
  - set JDK 21 if available
  - build host D1/D2 native targets, ideally only the metadata executables
  - build/run the Gradle host CLI
  - regenerate all checked-in mission metadata JSON
  - print the summary artifact path
- Keep `regenerate_all_mission_metadata.ps1` as the emulator parity path

Performance target:
- Current checkout has 109 `.zip`/`.7z` mission archives in `game_data/mission_files`, about 1.56 GB total
- A persistent D1 and D2 process should make the runtime startup cost roughly two native starts, not one per archive
- Main costs should become archive inspection/staging plus native level loading
- Target: under 60 seconds for the full folder on this workstation, with a warning in the summary if the run exceeds the target

Test plan:
- Kotlin unit tests for shared target discovery and projection:
  - mission ZIP with one descriptor/HOG
  - multiple mission sets in one archive
  - generic ZIP with loose level files
  - projection omits optional empty fields in the same order as current JSON
- Native tests:
  - build `dxx-redux-d1-headless-metadata` and `dxx-redux-d2-headless-metadata`
  - analyze one request JSON per game and assert `dxx-level-metadata-v1`
  - assert `route_steps` appears for at least one level
- Host integration test:
  - run host regeneration for a tiny allowlist such as one D1 pack and one D2 pack
  - compare selected fields against the current emulator-produced metadata shape
- Parity test:
  - periodically run one pack through both host and emulator helpers and compare normalized JSON
  - accept differences only when documented, ideally none

Risks and mitigations:
- Risk: native serializer drift between Android and host
  - Mitigation: extract one shared native serializer used by both paths
- Risk: checked-in JSON projection drift between launcher automation and host CLI
  - Mitigation: extract one shared Kotlin projection used by both paths
- Risk: request-to-request native state leakage in persistent batch mode
  - Mitigation: explicit mount/unmount tracking, smoke tests with two different missions in one process, and a measured subprocess fallback
- Risk: 7z handling differs between Android and host
  - Mitigation: shared archive interface with host-specific implementation and 7za fallback
- Risk: host path misses Android import side effects
  - Mitigation: keep emulator helper and add a small parity test, but do not make emulator the main regeneration route

Implementation phases:
- Phase 1: add host request mode to the existing headless metadata executable with the current native serializer, and prove one direct request can emit `dxx-level-metadata-v1`
- Phase 2: extract shared Kotlin request building and JSON projection, then add unit tests
- Phase 3: add the Gradle JVM host CLI and zero-parameter PowerShell wrapper
- Phase 4: add persistent native batch mode and performance reporting
- Phase 5: add parity tests and update `copilot-instructions.md` to prefer the host helper for normal metadata regeneration

Recommended first code step:
- Start with native single-request mode because it proves the most uncertain part: whether the host executable can consume the same request JSON and produce the same raw result as Android
- After that, the Kotlin CLI is mainly plumbing and normalization
