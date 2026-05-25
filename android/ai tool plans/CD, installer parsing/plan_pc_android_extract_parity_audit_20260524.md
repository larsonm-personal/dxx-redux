# PC/Android extract parity audit, 2026-05-24

## Goal

Audit every PC-side supported Descent/Descent II CD or installer extraction path against the Android extraction path, with special attention to Mac CDs, Infinite Abyss clean installs, and installer variants that may not share the same layout.

## Checklist

- [x] Read recent CD and installer parsing plan/history files
- [x] Identify the PC-side extraction entry points and supported disc/installer formats
- [x] Identify the Android extraction entry points and supported disc/installer formats
- [x] Build a per-CD/per-installer support matrix
- [x] Note gaps where PC support exceeds Android support
- [x] Note cases where support exists but depends on fragile assumptions
- [x] Recommend concrete follow-up fixes and tests

## Notes

- This is an audit-only tranche unless a small, obvious parity fix falls out naturally.
- Prefer source-backed conclusions over assumptions from plan text.

## Source-backed path map

### PC-side extract paths

- `game_data/extract_all_cds.ps1` builds and runs `android/app/src/main/cpp/extract/extract_cd.c` for every `game_data/CD images/*` folder with a `.cue` or `.iso`.
- `extract_cd.c` supports:
  - CUE sheets with one or more referenced image files, based on the CUE `FILE` lines.
  - Raw 2352-byte Mode 1 data tracks via `iso9660_reader.c`.
  - Standalone `.iso` files via `iso_list_image_files()` and `iso_extract_image_files()`.
  - Mac HFS data tracks via `hfs_reader.c` and `mac_hfs_extract.c`.
  - Mac STi/STi2 installer data forks via `sti2_extract.c`.
  - Recursive `.sow` expansion via `sow_extract.c`.
- `extract_cd.c` passes a `NULL` extension filter for ISO extraction, so the PC helper extracts the full ISO tree before `.sow` post-processing.
- `game_data/extract_all_gog.ps1` builds and runs `extract_gog.c`, which supports GOG Windows InnoSetup `.exe` and Mac XAR/gzip/cpio `.pkg`.
- Demo helper scripts use some external reference paths:
  - DOS demos: `game_data/extract_dos_demos.ps1` runs installers in DOSBox-X.
  - Mac demos: `game_data/extract_mac_demos.ps1` can use external `unar` as an oracle, while Android uses the in-tree StuffIt/STi path.

### Android-side extract paths

- `SetupActivity.kt` routes picked files:
  - `.cue` plus `.bin` or `.img` to `DiscImportDialog`.
  - `.inst` plus `.gog` to the same disc-image flow.
  - `.iso` to `IsoImportDialog`.
  - `.exe` or `.pkg` to `GogImportDialog`.
  - `.zip`, `.7z`, `.sit`, and `.hqx` to archive extraction.
  - `.sow` to direct SOW extraction.
- `import_cd` setup command supports `bin_paths`, including `.img`, and reorders selected image files to match CUE `FILE` entries before parsing.
- `DiscImportBridge` uses the same native CUE, ISO, HFS, STi/STi2, StuffIt, and SOW code as the PC helper.
- Android ISO extraction uses `dxx_android_disc_extract_extensions`, not a full-tree extract. This includes the gameplay payload types and `.sow`, then `postProcessImportedDiscFiles()` expands SOW archives and hoists nested game files to the active set root.
- Android Mac HFS extraction uses `dxx_android_mac_disc_extract_extensions`.
- Android GOG import uses the same native InnoSetup and `.pkg` readers as `extract_gog.c`, with direct fd support for InnoSetup installers and staged-file fallback for providers that do not expose a seekable fd.

## Current CD matrix

Legend:

- Runtime: whether the current Android import path should handle the same source files as `extract_cd`.
- Coverage: whether the current `extract_regression.json5` setup exercises Android extraction directly. Blank `import_mode` specs currently push pre-extracted `data_tracks` files, so they do not test Android extraction.
- Gap: the thing to fix first for parity confidence.

| Source | PC source shape | Runtime | Coverage | Gap |
| --- | --- | --- | --- | --- |
| d1 mac 2nd bin+cue | CUE + single BIN, HFS/STi, 14 tracks | Yes | No spec | Add spec and setup_cd regression |
| d2 mac | CUE + single BIN, HFS, 16 tracks | Yes | No spec | Add spec and setup_cd regression |
| Descent - Anniversary Edition (Brazil) (Covermount) | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Descent - Anniversary Edition (USA) | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Descent - Destination Saturn (USA) | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Descent - Levels of the World (USA) | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Descent - Mac macplay | CUE + single BIN, HFS/STi, 14 tracks | Yes | setup_cd | Covered |
| Descent - Test Flight (USA) | CUE + single BIN, ISO + 2 SOW | Yes | Push-only spec | Set setup_cd and verify SOW path |
| Descent (Europe) | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Descent (Europe) (Alt) | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Descent (USA) | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Descent Anniversary (ISO) | Standalone ISO | Yes | setup_iso | Covered |
| Definitive Collection EU Disc 1 | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Definitive Collection EU Disc 2 | CUE + 9 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd and multi-file audio/data regression |
| Definitive Collection EU Disc 3 | CUE + 8 BIN files, ISO | Yes | Push-only spec | Set setup_cd |
| Definitive Collection USA Disc 1 | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |
| Definitive Collection USA Disc 2 | CUE + 9 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd and multi-file audio/data regression |
| Definitive Collection USA Disc 3 | CUE + 8 BIN files, ISO | Yes | Push-only spec | Set setup_cd |
| Destination Quartzon Europe | CUE + 13 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Destination Quartzon USA | CUE + 13 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Destination Quartzon USA Diamond OEM | CUE + 13 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Destination Quartzon USA Logitech OEM | CUE + 13 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Destination Quartzon 3D Europe | CUE + 6 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Descent II - The Vertigo Series (USA) | CUE + 8 BIN files, ISO | Yes | Push-only spec | Set setup_cd |
| Descent II (Europe) | CUE + 13 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Descent II (Europe) (v1.1) | CUE + 9 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Descent II (USA) | CUE + 13 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Descent II (USA) (3-Level Interactive Preview) | CUE + single BIN, ISO + 3 SOW | Yes | Push-only spec | Set setup_cd and verify multi-SOW path |
| Descent II (USA) (Alt) | CUE + 13 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Descent II (USA) (Rerelease) | CUE + 9 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Descent II (USA) (v1.1) | CUE + 9 BIN files, ISO + SOW | Yes | Push-only spec | Set setup_cd |
| Descent II Infinite Abyss | CUE + 9 BIN files, ISO + `d2data/descent2.sow` | Yes | No spec | Add spec and setup_cd regression |
| Descent-II-Destination-Quartzon_Win_EN_ISO-Version | CUE + `.img`, plus `.ccd/.sub` sidecars | Yes | Push-only spec, source file list incomplete | Include `.img` in spec generation and set setup_cd |
| Dimensions for Descent (USA) | CUE + single BIN, ISO | Yes | Push-only spec | Set setup_cd |

## Installer/package matrix

| Source class | PC path | Android path | Status |
| --- | --- | --- | --- |
| GOG D1 Windows `.exe` | `extract_gog.c` InnoSetup | `GogImportBridge` InnoSetup | Runtime and D1 unified regression covered |
| GOG D2 Windows `.exe` | `extract_gog.c` InnoSetup | `GogImportBridge` InnoSetup | Runtime and redbook installer regression covered |
| GOG D1 Mac `.pkg` | `extract_gog.c` XAR/gzip/cpio | `GogImportBridge` `.pkg` | Runtime and D1 unified regression covered |
| GOG D2 Mac `.pkg` | `extract_gog.c` XAR/gzip/cpio | `GogImportBridge` `.pkg` | Runtime and redbook installer regression covered |
| DOS demo ZIP/SFX with SOW | DOSBox-X oracle helper | Android ZIP reader plus SOW append mode | Covered by demo package work, not part of CD matrix |
| Mac demo StuffIt/BinHex | external `unar` oracle and native tests | Android BinHex plus StuffIt/STi | Covered by native oracle tests, runtime validation still noted in prior plan |
| Loose `.sow` | native `sow_extract.c` | native `sow_extract.c` | Shared native support |

## Findings

1. Runtime parity for the current PC-supported CD image set looks good.
   - Every current CD image folder uses one of the Android-supported source shapes: CUE/raw image, standalone ISO, Mac HFS/STi, or ISO plus SOW.
   - The old `.img` and CUE ordering gaps from the 2026-05-21 audit are fixed in `SetupActivity.kt`: `.img` is recognized, selected images are reordered to CUE `FILE` entries, and `import_cd` accepts multiple `bin_paths`.
   - Infinite Abyss should work on Android now: the CUE data track is file index 0, Android extracts `.sow` from the ISO, then expands `d2data/descent2.sow` and hoists the resulting game files.

2. Test coverage is not at parity.
   - Only `Descent - Mac macplay` and `Descent Anniversary (ISO)` currently use direct Android extraction modes in their specs.
   - The other CD specs validate by pushing PC-extracted `data_tracks`, so a regression in Android CUE/ISO/SOW import would not be caught.
   - Three PC-extracted and known-disc CD folders have no `extract_regression.json5`: `d1 mac 2nd bin+cue`, `d2 mac`, and `Descent II Infinite Abyss`.

3. Spec generation is stale relative to launcher support.
   - `game_data/generate_regression_specs.ps1` records only `.bin` files for CUE sources, not `.img`. This leaves `Descent-II-Destination-Quartzon_Win_EN_ISO-Version` with a spec that names only `D2_DQ.cue`, even though `D2_DQ.img` is the required image file.
   - The generator only sets `import_mode = setup_cd` for `descent-mac-macplay`. It should set setup_cd for all CUE/image CD specs once direct import is the desired parity test.

4. PC and Android intentionally differ in ISO extraction breadth.
   - PC extracts the full ISO tree.
   - Android extracts a curated extension list, then expands SOW and hoists game files.
   - For all current CD folders this appears sufficient for supported gameplay files. The risk is future CDs whose only payload is inside an unsupported installer/archive format such as CAB, InstallShield, RAR, or a non-SOW ARJ.

5. Current unsupported image classes are not an Android-only gap.
   - CloneCD `.ccd/.sub` sidecars are ignored by both parity paths unless a CUE plus raw image is also present.
   - CHD/MDS/NRG and Mode 2/XA remain outside the current PC and Android extractor scope.

## Recommended follow-up

1. Update `game_data/generate_regression_specs.ps1` to include `.img` in `source_files` for CUE sources.
2. Change spec generation to set `import_mode = setup_cd` for every CUE/image CD spec, with an opt-out only for sources known to be too slow or non-launchable.
3. Generate specs for `d1 mac 2nd bin+cue`, `d2 mac`, and `Descent II Infinite Abyss`.
4. Run file-only direct-import coverage first:
   - `android/tests/test_all_extracts.ps1 -All -SkipLaunch`
5. Then run launch coverage for a representative subset:
   - one single-BIN D1 retail CD
   - one multi-BIN D2 retail CD
   - one SOW-heavy D2 CD
   - Infinite Abyss
   - d1 mac 2nd bin+cue
   - d2 mac
   - CUE+IMG Quartzon
6. Add a small regression target that scans all CD specs and fails if a CUE spec lacks the image files referenced by its CUE.

## Implementation progress

- [x] Updated `game_data/generate_regression_specs.ps1` to list `.img` alongside `.bin` for CUE/image CD sources.
- [x] Updated CD spec generation so every CUE/image CD spec uses `import_mode = setup_cd`; standalone ISO specs still use `setup_iso`.
- [x] Preserved existing `last_test_result` metadata when specs are regenerated.
- [x] Added game inference for specs whose known disc entry is missing or still says `unknown`.
- [x] Corrected `d1-mac-2nd-bincue` in `known_discs.json5` from `unknown` to `d1`.
- [x] Regenerated CD specs, including the missing specs for `d1 mac 2nd bin+cue`, `d2 mac`, and `Descent II Infinite Abyss`.
- [x] Added `android/tests/validate_extract_regression_specs.ps1` to verify CUE source lists and Android direct import modes.
- [x] Ran `android/tests/validate_extract_regression_specs.ps1`; it validated 34 CD regression specs.
- [x] Added shared canonical spec serialization in `android/tests/extract_regression_spec_helpers.ps1`.
- [x] Routed generation and `last_test_result` updates through canonical serialization.
- [x] Verified forced regeneration is timestamp-only after normalizing `// Generated:` lines.

Remaining runtime validation:

- [ ] Run `android/tests/test_all_extracts.ps1 -All -SkipLaunch` on an Android target.
- [ ] Run launch coverage for the representative subset listed above.

Environment note:

- `adb devices` could not run in this shell because `adb` is not on PATH, so Android target runtime validation was not started in this tranche.
