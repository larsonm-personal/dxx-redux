# Game File Format Registry Consolidation Plan

## Goal
- [x] Consolidate Kotlin-side game file format knowledge so extension normalization, labels, roles, game detection, mission descriptor parsing, archive summaries, and import-category decisions have one owner.

## Survey Status
- [x] Search Kotlin source for format strings such as `mn2`, `hog`, `pig`, `dxa`, `rl2`, and related helpers.
- [x] Identify competing format owners.
- [x] Draft a staged consolidation plan that keeps a superset of current behavior.
- [x] Implement consolidation.
- [x] Run focused and broad launcher tests.

## Current Competing Owners
- `AndroidGameFileExtensions.kt`
  - Owns import/extract extension sets and a private extension parser.
  - Duplicates knowledge now also present in `LauncherFileLabels.kt` and `GameFileMetadata.kt`.
- `LauncherFileLabels.kt`
  - Owns extension normalization via `launcherExtensionOf()`.
  - Owns user-facing labels and storage-purpose labels.
  - Also has saved-game extension knowledge.
- `GameFileMetadata.kt`
  - Owns HOG/DXA/PIG summaries.
  - Duplicates role classification, level-extension sets, and game detection.
  - Calls `SectorgameMissionZip.parseMissionDescriptor()`, so the "metadata" owner is not the parser owner.
- `SectorgameMissionZip.kt`
  - Owns mission ZIP scan structure and mission descriptor parsing.
  - Duplicates role classification and D1/D2 detection from extension and level names.
- `ModManager.kt`
  - Owns DXA mod archive categories, base replacement extension sets, texture/audio/docs extension sets, and mission-zip role labels.
  - Has its own `detectGame(filename)` for mod files.
- `SetupSections.kt`
  - Owns mission-zip role display labels.
  - Owns standalone descriptor and file metadata lookup glue.
- `FileSetManager.kt`
  - Owns per-set game-data extension list for cleanup/migration.
- `SetupActivity.kt`
  - Owns picker routing decisions by repeated `endsWith()` checks and direct calls to `SectorgameMissionZip.inspect()`.
- `multiplayer/MissionPicker.kt`
  - Owns mission descriptor extension choices and a separate mission descriptor parser.
- `DxaTextureScanner.kt`
  - Owns DXA ZIP scanning for texture dimensions. This is specialized enough to remain separate, but should use centralized extension/role facts for "is texture".

## Target Shape
- Introduce one Kotlin owner, tentatively `GameFileFormats.kt`.
- Keep it lightweight and launcher-facing; C loaders remain unchanged.
- `GameFileFormats` should own:
  - path leaf extraction and extension normalization, including DXA suffix rules.
  - extension facts: label, description, role, game hint, import/extract flags, mod-category hint, storage-category hint.
  - role labels for mission zip constituents and metadata summaries.
  - grouped extension sets derived from the registry, not hand-written in downstream files.
  - game detection helpers:
    - descriptor extension to game.
    - level extension to game.
    - file/list-of-files to game hint.
  - mission descriptor parsing.
  - saved-game extension generation.
- Keep `GameFileMetadata.kt` as the structured parser/summary service, but make it call `GameFileFormats` for:
  - extension normalization.
  - role labels.
  - level extension checks.
  - game detection.
  - mission descriptor parsing.
- Keep `DxaTextureScanner.kt` separate, but replace local texture-extension checks with `GameFileFormats.isTextureReplacement()` or equivalent.

## Proposed API Sketch
- `object GameFileFormats`
  - `fun extensionOf(path: String): String`
  - `fun leafName(path: String): String`
  - `fun typeLabel(path: String): String`
  - `fun extensionDescription(path: String): String`
  - `fun storagePurpose(file: File, relativePath: String, importedRootFile: Boolean): String`
  - `fun roleForFile(path: String): FileRole`
  - `fun roleLabel(role: FileRole): String`
  - `fun missionZipRoleForFile(path: String): MissionZipRole`
  - `fun gameForDescriptor(path: String): String?`
  - `fun gameForLevel(path: String): String?`
  - `fun gameHint(path: String, children: List<String> = emptyList()): String`
  - `fun parseMissionDescriptor(path: String, text: String): MissionDescriptor`
  - `fun hasGameImportExtension(path: String): Boolean`
  - `fun hasDiscExtractExtension(path: String): Boolean`
  - `fun isGameDataForSet(path: String): Boolean`
  - `fun isDxa(path: String): Boolean`
  - `fun stripDxaSuffix(path: String): String`
- Keep compatibility wrappers during migration:
  - `launcherExtensionOf()` delegates to `GameFileFormats.extensionOf()`.
  - `launcherFileTypeLabel()` delegates to `GameFileFormats.typeLabel()`.
  - `AndroidGameFileExtensions` delegates to `GameFileFormats`.
  - This keeps call sites stable while reducing the real source of truth.

## Implementation Tranche 1: Registry And Wrappers
- [x] Add `GameFileFormats.kt` with registry entries covering all current labels and extension sets.
- [x] Move `launcherExtensionOf`, DXA suffix logic, file labels, saved-game extension generation, and storage purpose logic into the registry.
- [x] Convert `LauncherFileLabels.kt` into compatibility wrappers or remove it after all call sites migrate.
- [x] Convert `AndroidGameFileExtensions.kt` into wrappers around registry-derived sets.
- [x] Add tests proving existing extension labels, DXA suffix behavior, saved-game labels, game import extension checks, and disc extract extension checks are unchanged.

## Implementation Tranche 2: Mission Descriptor And Mission ZIP
- [x] Move `SectorgameMissionZip.MissionDescriptor` and `parseMissionDescriptor()` into `GameFileFormats`.
- [x] Update `SectorgameMissionZip` to focus only on scanning a ZIP and deciding whether it is a mission bundle.
- [x] Replace `SectorgameMissionZip.roleForName()` with `GameFileFormats.missionZipRoleForFile()`.
- [x] Replace descriptor/level game detection with registry helpers.
- [x] Update `MissionPicker` to use the centralized descriptor parser instead of its local regex parser.
- [x] Keep existing mission picker built-in mission constants local, since those are engine mission-list facts rather than file-format facts.

## Implementation Tranche 3: Metadata And Mod Categorization
- [x] Update `GameFileMetadata` to use registry roles, labels, notes, and game hints.
- [x] Move `missionZipRoleLabel()` out of `SetupSections.kt` and into the registry.
- [x] Replace `ModManager` extension buckets with registry-derived categories:
  - base game file replacements.
  - texture replacements.
  - individual sound replacements.
  - music files.
  - documentation.
  - metadata patches stay local because they are mod-manifest structure, not generic file format.
- [x] Replace `ModManager.detectGame(filename)` with registry game hints.
- [x] Let mission zip category summaries use role labels from the registry.

## Implementation Tranche 4: Import, Storage, And Specialized Scanners
- [x] Update `SetupActivity.processPickedUris()` to switch on registry facts instead of repeated `endsWith()` checks where practical.
- [x] Update `FileSetManager.GAME_DATA_EXTENSIONS` to use registry-derived set-game-data extensions.
- [x] Update `DxaTextureScanner` texture entry checks to use centralized texture replacement facts.
- [x] Leave exact required asset filename lists in `SetupGameFiles.kt`; those are not generic format knowledge.
- [x] Leave demo package expected-file lists in `DemoInstallerPackages.kt`; those are package manifests, not generic format knowledge.
- [x] Leave C/native helpers alone, except for comments documenting where Kotlin duplicates engine constants.

## Functionality To Preserve
- Existing labels from `LauncherFileLabels.kt`, including `.ied`, `.txb`, `.sng`.
- Existing Android game import and disc extraction extension behavior.
- Existing mission zip detection and import behavior.
- Existing standalone `.mn2`/`.msn` info details.
- New HOG/DXA/PIG summaries from `GameFileMetadata`.
- Mod details category rollups and compatibility preflight.
- Multiplayer mission picker display names, level counts, and anarchy-only detection.
- DXA oversized texture warning behavior.

## Tests
- [x] Update/replace `AndroidGameFileExtensionsTest`.
- [x] Update/replace `LauncherFileLabelsTest`.
- [x] Keep and update `GameFileMetadataTest`.
- [x] Keep and update `MissionDescriptorFileDetailsTest`.
- [x] Keep and update `SectorgameMissionZipTest`.
- [x] Add focused `GameFileFormatsTest` coverage for shared descriptor parsing used by mission scanning.
- [x] Keep `ModManagerDetailsTest` and `ModManagerMissionZipTest` passing.
- [x] Run focused Gradle tests for all changed parser/format classes.
- [x] Run `android/run-code-quality.ps1 -Fix`.
- [x] Run `android/gradlew.bat :app:assembleDebug`.

## Completed Verification
- [x] `android/run-code-quality.ps1 -Fix -Paths ...` passed for changed Kotlin files.
- [x] `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.GameFileFormatsTest --tests com.dxxredux.app.AndroidGameFileExtensionsTest --tests com.dxxredux.app.LauncherFileLabelsTest --tests com.dxxredux.app.GameFileMetadataTest --tests com.dxxredux.app.MissionDescriptorFileDetailsTest --tests com.dxxredux.app.SectorgameMissionZipTest --tests com.dxxredux.app.ModManagerDetailsTest --tests com.dxxredux.app.ModManagerMissionZipTest --tests com.dxxredux.app.DxaTextureScannerTest` passed.
- [x] `android/gradlew.bat :app:assembleDebug` passed.

## Migration Notes
- Do not make UI code depend on file extensions directly unless it is rendering a literal filename.
- Exact file names required for D1/D2 readiness can stay in `SetupGameFiles.kt`.
- Exact package manifests can stay where they are.
- The registry should expose intent-level facts, not just raw extension sets, so downstream code asks "is this a texture replacement?" instead of checking `png/tga/ktx2` locally.
- If PIG parsing grows beyond summary-level header reads, move that parser to a native helper and keep only registry facts in Kotlin.
