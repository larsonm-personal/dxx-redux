# Plan: ISO Disc Image Data Loading

## Goal

Add support for loading Descent game data from standalone `.iso` files, with the same desktop-script, known-disc JSON, regression-spec, and `test_extract` coverage that the existing BIN/CUE path already has.

This is a data-only import path. BIN/CUE remains the only path that preserves Redbook audio tracks.

## Current State

- `game_data/extract_all_cds.ps1` only processes folders that contain a `.cue`
- `game_data/generate_regression_specs.ps1` only generates CD specs from `.cue` + `.bin`
- `game_data/hash_disc_tracks.ps1` only consumes `track_hashes.json` entries shaped like raw disc tracks
- `android/tests/test_extract.ps1` can drive direct launcher CD import, but the helper path assumes `cue_path` + `bin_path`
- `SetupActivity.kt` routes `.cue` and `.bin` into CD import, but does not recognize `.iso`
- `DiscImportBridge.kt` and `jni_disc_import.c` only expose BIN/CUE parsing plus ISO9660 extraction from a raw 2352-byte data track inside a BIN
- `iso9660_reader.c` already knows how to parse ISO9660, but its read path is hard-wired to raw Mode 1 sectors in a BIN data track
- `game_data/CD images/Descent Anniversary (ISO)/descent_anniversary.iso` exists now and is the right first corpus target

## Scope

In scope:

- Standalone `.iso` import for game data extraction
- Desktop extraction script support for `.iso`
- Known-disc metadata and regression-spec generation for `.iso`
- Direct launcher import and automated regression coverage for `.iso`
- Native tests for the direct ISO path

Out of scope:

- Redbook audio from `.iso`
- New image formats such as CHD, NRG, or MDS
- Reworking the existing BIN/CUE audio system beyond what is needed to keep behavior explicit

## Design Rules

1. Keep BIN/CUE and ISO as separate source types
2. Preserve BIN/CUE as the only audio-capable disc-image path
3. Treat standalone ISO as a single data track with 2048-byte logical sectors
4. Reuse existing ISO9660 traversal and extraction logic instead of adding a second filesystem parser
5. Keep the game code changes minimal and concentrate most of the work in `android/` and desktop tooling

## Proposed Data Model Changes

### 1. Distinguish source type in regression inputs

Extend CD-image metadata so specs and helper scripts can tell whether the source is:

- `cue_bin`
- `iso`

Recommended direction:

- Keep `source_type: "cd"` in `extract_regression.json5` so the higher-level test routing stays stable
- Add a second field such as `disc_image_type: "cue_bin" | "iso"`
- Allow `source_files` to contain either:
  - `cue` + one or more `bin` entries
  - one `iso` entry

This keeps old specs valid while making ISO explicit.

### 2. Extend `track_hashes.json` without breaking existing consumers

The current known-disc flow expects per-track SHA1 data. ISO has no audio tracks, but it still maps cleanly to a single synthetic data track.

Recommended direction:

- For standalone ISO, emit a one-entry `track_hashes.json`:
  - `track: 1`
  - `type: "data"`
  - `sha1: <sha1 of the full ISO file bytes>`
  - `files_extracted: <count>`
  - optional `source_format: "iso"`
- Keep the existing raw-track SHA1 semantics for BIN/CUE unchanged
- Teach the aggregation scripts and docs that a single-track ISO entry is a source-file fingerprint, not a raw 2352-byte track hash

This is the smallest way to keep `known_discs.json5` generation and disc matching useful for data-only ISO imports without pretending ISO has Redbook structure.

### 3. Teach `known_discs.json5` about ISO-only variants

Add ISO-only Descent 1 entries as normal single-data-track discs, with comments or a small metadata field making it clear that:

- the disc is data-only
- audio is unavailable from this source
- the SHA1 comes from the ISO file itself, not raw 2352-byte sectors

If desired, add a small optional field such as `source_format: "iso"` to known-disc entries for clarity.

## Implementation Phases

## Phase 1: Native direct-ISO read path

Goal: make the existing ISO9660 reader usable on a plain `.iso` file without routing through fake BIN track coordinates.

Steps:

1. Refactor `iso9660_reader.c/.h` so sector reads can come from either:
   - raw BIN sectors: 2352-byte sectors, user data at offset 16
   - direct ISO sectors: 2048-byte sectors, no raw-sector header
2. Pick one of these two implementations:
   - add a tiny reader-context struct that carries sector size, base offset, and data offset
   - or add parallel entry points such as `iso_list_files_from_file()` / `iso_extract_files_from_file()` that wrap the same directory walker
3. Keep the common directory walking and extraction logic shared
4. Update `jni_disc_import.c` with ISO-specific JNI entry points for direct file-descriptor extraction
5. Update `DiscImportBridge.kt` with matching wrappers for:
   - list files from an ISO fd
   - extract files from an ISO fd

Acceptance:

- native code can list and extract files from a standalone `.iso`
- existing BIN/CUE extraction still works unchanged

## Phase 2: Desktop extract tool and corpus scripts

Goal: make the repo-side extraction scripts generate the same committed artifacts for ISO folders that they already generate for BIN/CUE folders.

Steps:

1. Extend `android/app/src/main/cpp/extract/extract_cd.c` so it accepts either:
   - `extract_cd <cue_file> [output_dir]`
   - `extract_cd <iso_file> [output_dir]`
2. For ISO input:
   - compute the ISO SHA1 once
   - list and extract game files through the new direct-ISO path
   - emit the same newline-delimited JSON style used today, with a single data-track entry
3. Update `game_data/extract_all_cds.ps1`:
   - detect folders with `.iso` when no `.cue` exists
   - call `extract_cd.exe` on that ISO
   - keep output artifact names the same: `data_tracks/` and `track_hashes.json`
4. Run this on `game_data/CD images/Descent Anniversary (ISO)/` and commit the derived artifacts

Acceptance:

- ISO folders produce `data_tracks/` and `track_hashes.json`
- existing BIN/CUE folders remain unchanged

## Phase 3: Known-disc database and regression-spec generation

Goal: fold ISO sources into the same metadata pipeline used by committed CD-image fixtures.

Steps:

1. Update `game_data/hash_disc_tracks.ps1` to preserve any new `source_format` hint when promoting entries into `known_discs.json5`
2. Update `android/app/src/main/assets/DISC_HASHING.md` so it documents both hash modes:
   - BIN/CUE data and audio tracks use raw 2352-byte track hashes
   - standalone ISO uses whole-file SHA1 as a single data-track identity
3. Update `game_data/generate_regression_specs.ps1`:
   - stop hard-requiring `.cue`
   - detect `.iso` folders
   - hash ISO source files into `source_files`
   - write `disc_image_type: "iso"` for ISO specs
   - keep the rest of the expected extracted-file classification identical
4. Regenerate the spec for `Descent Anniversary (ISO)` so it has committed `extract_regression.json5`
5. Regenerate `known_discs.json5` to include the ISO corpus entry

Acceptance:

- the example ISO folder has committed `extract_regression.json5`
- `known_discs.json5` contains the ISO variant
- the docs explain the hash semantics clearly enough that future updates do not accidentally mix the two models

## Phase 4: Launcher import and SetupActivity automation

Goal: let the launcher import `.iso` directly and let automated tests drive that path.

Steps:

1. Update `SetupActivity.kt` file classification so `.iso` is recognized as a direct CD-image import source
2. Keep the UI copy explicit that:
   - `.cue/.bin` can provide game data plus audio
   - `.iso` provides game data only
3. Extend the broadcast command receiver used by tests so `import_cd` can accept either:
   - `cue_path` + `bin_path`
   - `iso_path`
4. Refactor the internal import helper so it branches by source type instead of assuming CUE/BIN
5. Extend `DiscImportBridge.kt` and the Kotlin import flow so direct ISO import goes through the new JNI direct-ISO extraction path
6. Make sure the post-import refresh and file-readiness checks stay shared

Acceptance:

- manual launcher import works for a standalone `.iso`
- existing BIN/CUE setup import still works
- automation broadcasts can drive both formats cleanly

## Phase 5: Regression tests

Goal: add the same level of committed regression coverage for ISO that BIN/CUE already has.

Steps:

1. Extend `android/tests/test_extract.ps1`:
   - allow direct setup-CD import specs to stage an ISO file instead of CUE/BIN
   - branch `Send-SetupCdImport()` or replace it with a format-aware helper
2. Add an ISO regression spec based on `game_data/CD images/Descent Anniversary (ISO)/extract_regression.json5`
3. Run the existing extract regression flow against that ISO spec
4. Extend `android/app/src/main/cpp/extract/test_cue_iso.c` with direct-ISO coverage:
   - list files from a direct ISO input
   - extract files from a direct ISO input
   - keep existing BIN/CUE synthetic coverage in place
5. If the native tests are split for clarity, keep both paths in the same executable so the build/test workflow stays simple

Acceptance:

- repo-side native tests cover direct ISO and BIN/CUE paths
- `android/tests/test_extract.ps1` can execute the ISO direct-import regression
- the committed ISO regression spec passes end to end

## Phase 6: Validation and cleanup

Goal: finish the tranche in a state that is easy to maintain.

Steps:

1. Run the relevant native build and tests for the extraction tool changes
2. Run the Android-side extraction regression for the ISO spec
3. Run `android/run-code-quality.ps1 --fix`
4. Verify that no existing BIN/CUE regression specs changed unexpectedly
5. Add short comments only where the two hash models or two import paths would otherwise be easy to confuse

Acceptance:

- ISO fixtures are committed and reproducible
- BIN/CUE behavior is unchanged
- docs and tests make the data-only limitation obvious

## Recommended Order of Execution

1. Native direct-ISO path in `iso9660_reader.c` and JNI bridge
2. Desktop `extract_cd` and `extract_all_cds.ps1`
3. `track_hashes.json`, `known_discs.json5`, and regression-spec generation
4. `SetupActivity.kt` and `test_extract.ps1`
5. Native and Android regression runs

## Risks and Decisions To Keep Explicit

1. ISO disc identity is not equivalent to a BIN raw-track hash
2. D2 `.iso` imports will not restore Redbook music, and the UI/test text should say so clearly
3. Do not overload the existing BIN/CUE parser with fake CUE generation unless the direct-ISO path turns out materially larger than expected
4. Keep the committed corpus focused at first on the existing `Descent Anniversary (ISO)` sample before broadening to more ISO variants

## Deliverables For This Tranche

- Updated native ISO reader path with direct-ISO support
- Updated `extract_cd` desktop tool
- Updated `extract_all_cds.ps1`, `hash_disc_tracks.ps1`, and `generate_regression_specs.ps1`
- Committed `track_hashes.json`, `data_tracks/`, and `extract_regression.json5` for `Descent Anniversary (ISO)`
- Updated `known_discs.json5` and `DISC_HASHING.md`
- Updated `SetupActivity.kt`, `DiscImportBridge.kt`, and `android/tests/test_extract.ps1`
- Extended `test_cue_iso.c` coverage

## Phase 1-2 Progress

- [x] `iso9660_reader.c/.h` now supports both raw BIN data tracks and standalone ISO images through a shared source abstraction
- [x] `extract/jni_disc_import.c` and `DiscImportBridge.kt` expose direct ISO file listing and extraction entry points alongside the existing BIN-track APIs
- [x] `extract_cd.c` now accepts either a `.cue` or a `.iso` input and emits a single data-track JSON line for ISO sources
- [x] `game_data/extract_all_cds.ps1` now detects standalone `.iso` folders when no `.cue` is present
- [x] `test_cue_iso.c` now covers direct ISO listing and extraction in addition to the existing BIN/CUE path
- [x] Validation completed:
   - desktop CMake build of `test_cue_iso` and `extract_cd`
   - `ctest -C Release -R cue_iso_tests`
   - real `extract_cd.exe` smoke run against `Descent Anniversary (ISO)` with 507 extracted files and ISO SHA1 `cc63e0eeb180678eb154b2d5f10ee6b8e25b9e4b`
   - `android\\gradlew.bat :app:assembleDebug --console=plain` with `JAVA_HOME=c:\\local\\jdk-21`

## Phase 3-5 Progress

- [x] `game_data/hash_disc_tracks.ps1` now preserves `source_format` when promoting ISO entries into `known_discs.json5`
- [x] `game_data/generate_regression_specs.ps1` now supports ISO-only folders, emits `disc_image_type: "iso"`, and writes `import_mode: "setup_iso"` for standalone ISO specs
- [x] `generate_regression_specs.ps1` now uses `-LiteralPath` for source hashing so bracketed sample names such as `Descent [Mac].BIN` do not break spec generation
- [x] `android/app/src/main/assets/DISC_HASHING.md` and `known_discs.json5` now document the standalone ISO single-track hash model
- [x] `game_data/CD images/Descent Anniversary (ISO)/` now has committed `track_hashes.json`, `data_tracks/`, and `extract_regression.json5`
- [x] `known_discs.json5` now includes `descent-anniversary-iso` with track 1 SHA1 `cc63e0eeb180678eb154b2d5f10ee6b8e25b9e4b` and `source_format: "iso"`
- [x] `SetupActivity.kt` now recognizes `.iso` picks, exposes `SETUP_COMMAND import_iso`, and routes standalone ISO imports through a dedicated data-only dialog and shared path helper
- [x] `android/tests/test_extract.ps1` now stages standalone ISO sources and drives the launcher with `import_mode: "setup_iso"`
- [x] Desktop/native validation completed:
   - `android\\tests\\build\\Release\\test_cue_iso.exe` passed 48/48 tests when launched from `android\\`
   - `android\\run-code-quality.ps1 --fix` completed through formatting/lint passes and the embedded `:app:assembleDebug` build succeeded
- [ ] Android device regression is still pending because `adb devices` returned no attached emulator or device in this session

## Status

- [x] Current BIN/CUE-only touchpoints identified
- [x] First ISO corpus target identified
- [x] Native direct-ISO path implemented
- [x] Desktop scripts updated
- [x] Regression spec and known-disc metadata updated
- [x] Launcher import updated
- [x] Desktop/native regression coverage updated
- [ ] On-device ISO regression run completed
- [x] Phase 1-2 tests added and passing