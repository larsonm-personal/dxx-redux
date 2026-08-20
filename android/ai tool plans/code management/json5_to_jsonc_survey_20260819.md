# JSON5 to JSONC conversion survey

## Scope

Inventory every maintained `.json5` file and every active reader, writer, discovery rule, test, configuration, packaging rule, and documentation reference that must move to `.jsonc`.

## Plan

- [x] Read repository instructions and confirm the working tree baseline
- [x] Inventory tracked and relevant ignored `.json5` files by data family
- [x] Trace active consumers, producers, discovery rules, packaging, and parser helpers
- [x] Check whether maintained files use JSON5 syntax beyond JSON with comments
- [x] Identify validation coverage, migration ordering, and implementation risks
- [x] Record the completed survey findings and recommended implementation phases

## Constraints

- Survey only. Do not rename data files or alter consumers and producers in this tranche
- Preserve handmade comments during the later conversion
- Do not add backward compatibility readers for disposable Android formats unless a non-Android or external interface requires it

## Inventory

There are 149 tracked files ending in `.json5` and five additional tracked template or default files whose names contain `.json5`.

| Family | Count | Notes |
| --- | ---: | --- |
| `android/game_scripts` | 70 | Automation scripts, including host-resolved templates |
| `game_data/CD images` | 34 | `extract_regression.json5` files |
| `game_data/combined launches` | 2 | One combined-launch descriptor and one extraction regression spec |
| `game_data/gog installers` | 4 | Installer regression specs |
| `game_data/music` | 33 | Per-album `chromaprint_info.json5` sidecars |
| `game_data/mission_files` | 1 | CD mission metadata source manifest |
| `android/app/src/main/assets` | 4 | Known versions, discs, albums, and fingerprint configuration |
| `android/benchmarks` | 1 | Level metadata benchmark manifest |
| `android` | 1 | Ignored local AcoustID configuration present in this checkout |
| Tracked suffix files | 5 | Android auth/AcoustID examples and three server defaults/templates |

The ignored local `android/acoustid_config.json5` must be preserved and renamed without displaying or rewriting its secret value. No ignored live auth or server configuration was present during the survey. Generated `.resolved`, build-intermediate, and root `temp` copies also exist. They should be regenerated or narrowly cleaned after their producers use `.jsonc`; they are not source data to edit individually.

The five suffix files should be reordered so `.jsonc` is the final extension and VS Code actually selects its JSONC language mode: `auth_config.template.jsonc`, `acoustid_config.example.jsonc`, `config.default.jsonc`, `config.lan.jsonc`, and `server_config.template.jsonc`. A mechanical rename such as `auth_config.jsonc.template` would not deliver the editor benefit that motivated the task.

The repository already contains the completed precedent: `robot_names_d1.jsonc` and `robot_names_d2.jsonc`. `RobotNameCatalog` still calls the shared `Json5.strip` helper, so the shared parser vocabulary has not yet been converted.

## Syntax findings

All tracked JSON5-named files were checked by removing comments and trailing commas outside strings and then using a strict JSON parser.

- 147 of 154 are compatible with JSON plus comments and optional trailing commas
- 150 contain actual line comments
- none contain block comments
- four contain trailing commas
- seven need more than an extension rename

Files with genuine JSON5 syntax that must be normalized:

- `android/game_scripts/profile_uneasy4_level_preview.json5`: unquoted keys and trailing commas
- `android/game_scripts/test_level_metadata_result_cache_reuse.json5`: unquoted keys and trailing commas
- `server/config.json5.default`: unquoted keys and trailing commas
- `server/config.json5.lan`: unquoted keys and trailing commas

Templates that are intentionally invalid before substitution:

- `android/game_scripts/test_base_robot_preview.json5`: raw `${ROBOT_NUMBER}` value
- `android/game_scripts/test_robot_preview.json5`: raw `${SEED}` value
- `android/game_scripts/test_extract_regression_template.json5`: raw `MISSION_OPTIONAL` value

For useful VS Code validation, the template placeholders should become quoted sentinel strings and their producers should replace the complete quoted sentinel with a serialized JSON boolean or number. Merely renaming these three files would leave them invalid JSONC in the editor.

## Consumers and parsers

### Automation scripts

- Discovery and dispatch: `android/run_all_tests.ps1`, `android/Run-TestMenu.ps1`, `android/run_quick_tests.ps1`, `android/helpers/run_automation.sh`, `android/helpers/run_test.ps1`, and multiplayer/specialized test wrappers
- Host parsing and resolution: `android/helpers/json5.ps1` and the JSON5 functions and comments in `android/helpers/test_helpers.ps1`
- Launcher parsing: `Json5.kt` and the separate `stripLauncherJson5` implementation in `LauncherScriptExecutor.kt`
- Native game parsing: `game_automate.cpp` consumes the resolved script using nlohmann JSON with comment acceptance
- Dynamic script producers: `run_mission_zip_batch.ps1`, `test_extract.ps1`, `test_random_level_preview.ps1`, `test_robot_preview.ps1`, and `Resolve-TestScript` in `test_helpers.ps1`

The format labels used for test catalog entries, report grouping, timeout helpers, and test fixtures also use the string `json5`. These are internal identifiers rather than extensions, but they should move to `jsonc` so the migration is complete and future searches remain reliable.

### Android assets and private configuration

- `KnownVersions.kt` loads `known_versions.json5`
- `DiscIdentifier.kt` and `FingerprintBridge.kt` load `known_discs.json5`, `known_albums.json5`, and `fingerprint_config.json5`
- `AcoustIdClient.kt` loads the build-generated `acoustid_config.json5`
- `android/app/build.gradle` reads ignored auth and AcoustID configs and produces the packaged AcoustID asset
- `AcoustIdConfiguration.kt`, `FingerprintBridge.kt`, `RobotNameCatalog.kt`, and tests call `Json5.strip`
- Android manifest comments, C/C++ fingerprint comments, error messages, and Kotlin tests name the old files

`Json5.kt` should become the common `Jsonc` comment cleaner. The duplicate launcher cleaner also removes trailing commas, while `Json5.strip` only removes comments. The implementation should either deliberately keep those responsibilities separate under JSONC names or centralize them with tests for URL strings, escaped quotes, comment markers inside strings, trailing commas, and unterminated block comments.

### Regression and generated data

- `game_data/hash_assets.ps1` reads and writes `known_versions.json5` with its own JSON5-named helpers
- `game_data/update_known_discs_albums.ps1` reads all `chromaprint_info.json5` files and writes `known_albums.json5`
- `game_data/fingerprint_mission_zip_music.ps1` writes chromaprint sidecars, reads the ignored AcoustID config, and invokes the album publisher
- `game_data/generate_regression_specs.ps1` reads known discs and combined-launch descriptors and writes CD and GOG regression specs
- `android/tests/extract_regression_spec_helpers.ps1` owns canonical extraction-spec reading and writing
- `game_data/extract_all_cds.ps1`, `game_data/extract_all_gog.ps1`, and `game_data/run_all_cd_regressions.ps1` discover specs by the old extension
- Mission metadata regeneration and the level metadata benchmark load their old manifest names

`combined_launch.json5` contains relative paths to `extract_regression.json5` files. These payload strings must be updated along with the files.

### Matchmaking server

- Rename all three tracked server defaults/templates and the ignored runtime filename to `.jsonc`
- Update `CONFIG_FILE`'s default in `server/src/config.rs`, deployment scripts, certificate helper output, service comments, and `.gitignore`
- Quote the keys and remove trailing commas in the two defaults that use genuine JSON5 syntax
- The server currently parses with the pinned `json5` crate. Since `serde_json` is already a dependency, implementation can replace `json5` with a small tested comment-cleaning step plus `serde_json`, or retain the permissive parser under JSONC filenames. Removing the JSON5 crate gives the clearest enforcement that only JSONC syntax is supported and updates `Cargo.toml` and `Cargo.lock`

### Repository metadata and documentation

- Update `.editorconfig` to associate `jsonc` rather than `json5`
- Update root and server `.gitignore` patterns
- Update active instructions in `.github/copilot-instructions.md`
- Update current documentation and comments such as `DISC_HASHING.md`, demo installer README, Android manifest, and `outstanding_bugs.md`
- Historical files under `android/ai tool plans` contain many old `.json5` references. They are archival evidence rather than consumers. Rewriting them would create a large misleading history-only diff, so the recommended scope is to leave them unchanged and exclude that directory from the final active-reference check

## Recommended implementation phases

- [x] Rename shared parser helpers and tests from JSON5 to JSONC, preserving string-safe comment and trailing-comma behavior
- [x] Normalize the four genuine JSON5 documents and make the three templates valid JSONC before substitution
- [x] Rename automation scripts and update all discovery, dispatch, dynamic output, device-copy, catalog, and report identifiers
- [x] Rename Android assets and private config examples, then update Gradle packaging, Kotlin/C consumers, tests, and the present ignored AcoustID config
- [x] Rename regression, music, combined-launch, mission-source, and benchmark data; update generators, embedded relative paths, canonical writers, and batch discovery
- [x] Rename server configuration files and references, normalize syntax, and remove the JSON5 parser dependency
- [x] Update active repository metadata and documentation, regenerate derived outputs, and run a final active-tree sweep

Implementation status: complete as of 2026-08-19

Do each phase as an atomic producer-plus-consumer change. Renaming all data first would temporarily break discovery and makes it easier to miss dynamically generated filenames.

## Validation targets

- Static check: no tracked filename outside historical plans contains `.json5`
- Static check: no active source, test, config, or documentation reference outside historical plans contains `json5`
- Parse every maintained `.jsonc` after comment/trailing-comma cleaning with a strict JSON parser
- Run the renamed JSONC parser and automation catalog tests
- Run focused extraction workflow, spec generation/validation, asset hashing, fingerprint publication/configuration, and AcoustID packaging tests
- Run Android/Kotlin unit tests covering assets and parser behavior
- Run server lint/build/tests after the config parser and dependency change
- Run the scoped code-quality wrapper over all changed scripts, Kotlin, Gradle, C/C++, Rust, JSONC, and metadata files
- Run the required Windows CMake build/test path and one representative D1 and D2 automation script under its new `.jsonc` name
- Confirm generated `.resolved`, APK asset, and temporary automation filenames now use `.jsonc`

## Implementation validation

- All 156 tracked JSONC documents pass strict JSON parsing after string-safe comment removal
- The ignored local AcoustID configuration was renamed to `.jsonc`, kept private, and parsed successfully
- No tracked filename contains `json5`
- No active tracked source, test, configuration, or documentation reference contains `json5`; historical AI plans and the protected outstanding-bugs checklist remain unchanged
- JSONC parser, automation catalog, extraction workflow, extraction generation, fingerprint, AcoustID packaging, asset hashing, CD metadata source, and CD regression runner tests passed
- Android JVM unit tests passed under JDK 21
- The scoped multi-language code-quality pass over 244 changed paths passed
- The server Rust lint/build/test wrapper passed 34 unit tests, 59 integration tests, and 13 NAT simulator tests; it reported one pre-existing Clippy warning in `ws_handler.rs`
- The Windows host build completed for D1, D2, and their headless targets
- `test_launch_to_automap.jsonc` passed on the emulator for both D1 and D2
