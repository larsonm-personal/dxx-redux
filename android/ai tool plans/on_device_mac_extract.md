# On-Device Mac CD Extraction (Android)

Status: Planning expanded on 2026-04-11. Six implementation tranches are now
in: HFS/APM detection, HFS catalog plus root-file extraction, STi2 entry
listing, STi2 method 13 extraction, JNI plus launcher fallback
integration, and setup-command direct disc-import regression coverage.
The remaining gaps are the full SAF picker UI path and the secondary-disc pass.

## Goal

Enable Android users to import game files directly from a Mac HFS CD image
on-device, without needing a PC extraction step. The first target is the
Descent 1 MacPlay CD. The native path should stay generic enough to support
later Mac HFS discs too.

## Planning tranche complete

- [x] Identify the real source media already present in `game_data/`
- [x] Record source-media hashes for the primary MacPlay disc and the known
   second Mac BIN/CUE variant
- [x] Record the known-good extracted MacPlay output hashes already present in
   `data_tracks/`
- [x] Cross-check nearby solved GOG installer paths that can serve as JNI and
   test-pattern references
- [x] Decide which upstream codebases are the most likely inspiration sources
   for future AI-generated files, and what attribution policy should follow

## Current state

- Desktop reference extractor already works: `game_data/extract_mac_cd.ps1`
- Android already has adjacent import paths we can mirror:
   - BIN/CUE ISO 9660: `cue_parser.c`, `iso9660_reader.c`, `jni_disc_import.c`
   - GOG `.exe` and `.pkg`: `inno_reader.c`, `pkg_reader.c`, `jni_gog_import.c`,
      `extract_gog.c`
- `known_discs.json5` already contains both Mac D1 disc variants:
   - `descent-mac-macplay`
   - `d1-mac-2nd-bincue`
- `known_versions.json5` already contains D1 Mac (MacPlay) hashes for:
   - `demo1.dem`
   - `descent.hog`
   - `descent.pig`
   - `watchme.dem`
   - `yep9.dem`
- `hash_assets.ps1` already maps folder name `Descent - Mac macplay` to
   version `D1 Mac (MacPlay)`
- Gap to close during implementation:
   - Mac `CHAOS.MSN` hash is known, but `.msn` is not currently included in the
      `hash_assets.ps1` extension list
- Initial native HFS tranche now exists:
   - `android/app/src/main/cpp/extract/hfs_reader.c`
   - `android/app/src/main/cpp/extract/hfs_reader.h`
   - `android/app/src/main/cpp/extract/test_hfs.c`
   - `extract_cd.c` now reports HFS tracks as HFS instead of a generic ISO
      failure when `Apple_HFS` is detected
- Current native HFS coverage now also includes:
   - catalog leaf-node scanning for classic HFS catalog files
   - directory and file listing with reconstructed slash-separated paths
   - direct data-fork extraction for files that fit within the first extent
      record set
   - a verified extraction of the root-level `Install Descent` archive with
      `STi2` magic on the primary MacPlay disc
- Current native STi2 coverage now also includes:
   - `android/app/src/main/cpp/extract/sti2_extract.c`
   - `android/app/src/main/cpp/extract/sti2_extract.h`
   - `android/app/src/main/cpp/extract/test_sti2.c`
   - CRC-validated entry-header scanning over the real MacPlay archive
   - parent-linked path reconstruction for the archive's internal directories
   - verified listing of the 8 regular MacPlay outputs already present in
      `data_tracks/`
   - method 13 data-fork decompression for STi2 entries
   - `sti2_extract_entry()` and `sti2_extract_matching()`
   - verified extraction of all 8 regular MacPlay outputs byte-for-byte
      against the existing `data_tracks/` oracle files

## 2026-04-12 implementation findings

- `hfs_find_partition()` is implemented and currently covers:
   - Apple driver descriptor record detection
   - Apple partition map scanning
   - `Apple_HFS` partition identification
   - HFS MDB parsing for volume name and basic sizing fields
- `test_hfs` now validates:
   - a synthetic non-ISO, non-sector-aligned HFS partition case
   - primary MacPlay disc detection
   - secondary Mac BIN/CUE variant detection
   - primary MacPlay catalog listing
   - extraction of root-level `Install Descent` with `STi2` header bytes
- Verified on both real D1 Mac discs:
   - partition map entry count: 2
   - HFS partition block count: 319989
   - physical block size: 512
- Verified on the primary MacPlay disc:
   - `extract_cd.exe` now emits
      `{"filesystem": "hfs", "partition_blocks": 319989}`
     for track 1 instead of an ISO error
   - `hfs_list_files()` finds root-level `Install Descent` and `MacPlay Demos`
   - `hfs_extract_file()` can extract `Install Descent` directly from the HFS
      volume and the output starts with `STi2`
   - the final `Descent` executable should no longer be treated as the first
      direct-HFS proof point on this disc; the MacPlay desktop pipeline's final
      `Descent` file appears downstream of the installer path rather than as the
      first native HFS target to chase here
- Still not implemented in native code:
   - extents-overflow lookup for files needing more than the first three extents
   - JNI and Kotlin integration

## 2026-04-12 STi2 tranche findings

- `sti2_list_entries()` is now implemented and covered by `test_sti2`
- The primary MacPlay `Install Descent` archive is not a simple
   `archive-header -> file-header -> payload -> next file-header` stream from
   byte 22
- The real archive is better handled by scanning for CRC-valid visible file
   headers and reconstructing paths from parent offsets, rather than assuming a
   purely sequential walk
- The visible file headers are 112-byte StuffIt-family records with the same
   key field offsets used by `XADStuffItParser.m` for name length, file type,
   creator, dates, and compressed or uncompressed sizes
- The parser tranche was written from the real-disc bytes plus the published
   field layout. The later method 13 decompressor takes its primary
   inspiration from `XADStuffIt13Handle` and `XADPrefixCode`, so
   `sti2_extract.c` now carries an explicit LGPL-2.1 provenance note
- The landed method 13 path supports both the fixed-table modes 1-5 and the
   dynamic meta-coded mode 0 used by classic StuffIt family streams
- Verified on the primary MacPlay archive:
   - archive magic `STi2`
   - declared header count `13`
   - 8 regular data-fork outputs are listed and match the known `data_tracks/`
      file sizes
   - all 8 regular data-fork outputs now extract byte-for-byte identical to
      the known `data_tracks/` files via `test_sti2`
   - confirmed visible header offsets:
      - `CHAOS.HOG` at `0x69398`
      - `CHAOS.MSN` at `0x7E595`
      - `descent.hog` at `0x7E767`
      - `descent.pig` at `0x456260`
      - `demo1.dem` at `0x5C0FC9`
      - `watchme.dem` at `0x62EC5A`
      - `yep9.dem` at `0x6837EB`
   - the archive also contains directory headers such as `Data` and `Demos`,
      so later extraction should match by path or basename intentionally rather
      than assuming a flat top level

## Verified source media

### Primary implementation target: Descent - Mac macplay

| Artifact | Location | Size | SHA256 |
|---|---|---:|---|
| BIN | `game_data/CD images/Descent - Mac macplay/Descent - Mac macplay.bin` | 718912320 | `38393a12630bbdfdd2e2efac138da463c242586a42c889b91054fabf6ba7b925` |
| CUE | `game_data/CD images/Descent - Mac macplay/Descent - Mac macplay.cue` | 1125 | `b8127c0ce27a4573b596c5cfb78af0f0e7ce58e1dbe130e1424f3ede09d4446d` |

Verified disc facts:
- Folder: `game_data/CD images/Descent - Mac macplay/`
- `track_hashes.json` already exists
- Known disc id: `descent-mac-macplay`
- Data track SHA1: `fd32cb88782068aab7bc98b8920672e36637da80`
- Track layout: 1 data track (Mode1/2352) + 13 redbook audio tracks
- Desktop probe already confirmed why the normal ISO path fails:
   track 1 reports `ISO listing failed`, which is correct for an HFS data track
- HFS/APM details already observed by the desktop investigation:
   - DDR block size 512
   - partition map contains `Apple_partition_map` and `Apple_HFS`
   - HFS volume name `Descent`
   - Apple_HFS partition length 319989 blocks

### Secondary compatibility target: d1 mac 2nd bin+cue

| Artifact | Location | Size | SHA256 |
|---|---|---:|---|
| BIN | `game_data/CD images/d1 mac 2nd bin+cue/Descent [Mac].BIN` | 718912320 | `3f921a15abc3dc656d98408096d344414e434dca52f1a7e124fd310987716030` |
| CUE | `game_data/CD images/d1 mac 2nd bin+cue/Descent [Mac].CUE` | 612 | `0717b611f4ea8da1140d7ad9a53605ffaf62362148e81e7a91705e78bb8de9b0` |

Verified disc facts:
- Folder: `game_data/CD images/d1 mac 2nd bin+cue/`
- `track_hashes.json` already exists
- Known disc id: `d1-mac-2nd-bincue`
- Data track SHA1: `527579d7c267f070abeafd1e9d0efc7d945b446d`
- This should be treated as a follow-up compatibility target, not a first-pass
   blocker. First implementation should lock onto the primary MacPlay disc.

### Nearby solved installer inputs worth mirroring

These are not the target input for this feature, but they are the closest
existing examples of the current Android import style and test flow.

| Artifact | Location | Size | SHA256 | Why it matters |
|---|---|---:|---|---|
| D1 Mac GOG `.pkg` | `game_data/gog installers/descent_enUS_1_0_35122.pkg` | 22251081 | `d0720e12b95cfaee95e133ae08249ef05fcec17c6e9ca362bd54d8efb96f2293` | Existing solved Mac installer path via `pkg_reader.c` |
| D1 Windows GOG `.exe` | `game_data/gog installers/setup_descent_1.4a_(16596).exe` | 26384096 | `cd754293de928f73a3772630a85d274b22940c857b1aab80078afa962a5492a7` | Existing solved Windows installer path via `inno_reader.c` |

Both of those solved paths already extract the same standard 7-file D1 full set
under their `extracted/` folders, so they are good JNI and regression-pattern
references, but not good asset-hash oracles for MacPlay because MacPlay uses
different core assets.

## Verified expected outputs for the primary disc

Current known-good desktop output folder:
`game_data/CD images/Descent - Mac macplay/data_tracks/`

Current expected files and hashes:

| File | Size | SHA256 | Why it matters |
|---|---:|---|---|
| `Descent` | 638827 | `027398a8ee53fbcc4956a8308d075a729e58034a3ff8aa6dfeaaae57ebbb4145` | Final imported Mac executable. Keep this hash as a downstream validation target, but do not assume it is the first direct-HFS proof point on the primary MacPlay disc |
| `CHAOS.HOG` | 174751 | `0c5fb0684443c0b9c4dbb2769db65b8460bdad4ca80d58a943396fd07c001ba4` | Small STi2 output. Byte-identical to PC D1 v1.0 `chaos.hog` |
| `CHAOS.MSN` | 299 | `be614df3ab4d35350f1033fc4acd7f18f779883273cdbf1db1d976df4be11d02` | Smallest Mac-specific STi2 output. Good early decompressor oracle |
| `demo1.dem` | 1244458 | `182b0f6e209f6e4b5d34fe7c1cbe9222425f3295f6dfe50917fb0bc0cf6cde6a` | Medium-sized STi2 output already present in `known_versions.json5` |
| `descent.hog` | 7456179 | `d0ab72fda672ac9d751d24899f96ac14eb1720386cf2b0226a5add71543c4339` | Required final game asset |
| `descent.pig` | 3975533 | `9eb232c9da830309b2d3a1de75713f6155c62215cd867f4570536180af31f299` | Required final game asset |
| `watchme.dem` | 1015201 | `a1d8fd365022a0ca56026d6a9325a6dbd41e483f76e6ec8c13d844a13ce24c87` | Medium-sized STi2 output already present in `known_versions.json5` |
| `yep9.dem` | 906688 | `20ee0e297f114b1ded6c1156db7247479866d30556598992dfd9ae245deee49b` | Medium-sized STi2 output already present in `known_versions.json5` |

Important comparisons against the already-solved D1 GOG outputs in
`game_data/gog installers/setup_descent_1.4a_(16596)/extracted/` and
`game_data/gog installers/descent_enUS_1_0_35122/extracted/`:

- `CHAOS.HOG` matches exactly across MacPlay and the PC/GOG set
- `CHAOS.MSN` does not match:
   - MacPlay: 299 bytes, `be614df3ab4d35350f1033fc4acd7f18f779883273cdbf1db1d976df4be11d02`
   - PC/GOG: 309 bytes, `9f6ed0de3b9c5aea3f609598fc8cd3ae959594a1399d38714b5fd3d9522de819`
- `descent.hog` and `descent.pig` are different asset versions from the PC/GOG set
- The demo set is different too:
   - MacPlay: `demo1.dem`, `watchme.dem`, `yep9.dem`
   - PC/GOG: `DESCENT.DEM`, `LEVEL18.DEM`, `MINIBOSS.DEM`

Output casing to preserve, because the desktop oracle already established it:
- `CHAOS.HOG`
- `CHAOS.MSN`
- `demo1.dem`
- `Descent`
- `descent.hog`
- `descent.pig`
- `watchme.dem`
- `yep9.dem`

Note:
- `Install Descent` is now the proven direct-HFS extraction target on the
   primary MacPlay disc.
- `Descent` remains useful as a downstream validation target and for
   provenance, but it is not required for launcher readiness. The Android
   production import path may decide to skip copying it into the set directory
   after the STi2 path is proven.

## Desktop oracle to mirror in native code

Working reference pipeline today: `game_data/extract_mac_cd.ps1`

Stage boundaries and intermediate files are already clear:
1. Parse CUE and find the data track
2. Strip Mode1/2352 sectors into `_mac_extract_temp/data_track_raw.img`
3. Parse the Apple Partition Map
4. Copy the HFS partition into `_mac_extract_temp/hfs_partition.img`
5. Use `machfs` to dump the HFS volume into `_mac_extract_temp/hfs_files/`
6. Find the `Install Descent` data fork and pass it to `unar` into
    `_mac_extract_temp/unar_output/`
7. Copy matching game files into `data_tracks/`

Important behavior to keep in mind when translating this to native Android:
- the desktop script reads the HFS image into Python memory in one shot via
   `machfs`; the Android C path should stay streaming or chunked
- the desktop script supports both assets directly on HFS and assets inside the
   STi2 archive; this matters because later Mac discs such as D2 Mac may put
   some files directly on HFS outside the installer
- the desktop script searches for `Install Descent` by common names first, then
   falls back to STi2 magic scanning; the native path should do the same

## Planned native files and touch points

Likely new files:
- `android/app/src/main/cpp/extract/hfs_reader.c`
- `android/app/src/main/cpp/extract/hfs_reader.h`
- `android/app/src/main/cpp/extract/sti2_extract.c`
- `android/app/src/main/cpp/extract/sti2_extract.h`
- `android/app/src/main/cpp/extract/test_hfs.c`
- `android/app/src/main/cpp/extract/test_sti2.c` or equivalent extension of the
   existing extract test harness
- `game_data/CD images/Descent - Mac macplay/extract_regression.json5`

Likely existing files to touch:
- `android/app/src/main/cpp/extract/CMakeLists.txt`
- `android/app/src/main/cpp/CMakeLists.txt`
- `android/app/src/main/cpp/extract/jni_disc_import.c`
- `android/app/src/main/java/com/dxxredux/app/DiscImportBridge.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

Likely existing test entry points to reuse:
- `android/tests/test_extract.ps1`
- `game_data/generate_regression_specs.ps1`

## Native architecture

Follow the existing Android extraction pattern, but add a Mac-specific HFS and
STi2 branch:

```
Kotlin SetupActivity
   -> DiscImportBridge.kt
      -> jni_disc_import.c
         -> cue_parser.c (existing)
         -> hfs_reader.c (new)
         -> sti2_extract.c (new)
```

The Kotlin bridge should ideally mirror the current ISO and GOG UX:
- a detection/listing path so the launcher can preview what will be imported
- an extraction path with progress callbacks

At minimum, new bridge methods will likely be:
- `nativeListMacCdFiles(...)`
- `nativeExtractMacCd(...)`

Detection rule in `SetupActivity.kt`:
- if ISO listing fails, check for Apple DDR signature `0x4552` on the data track
- if that is present, route through the Mac HFS path instead of treating the
   disc as a normal ISO failure

## Component plan

### A. HFS reader (read-only) -- feasible

Implementation status after tranche 2:
- [x] Apple Partition Map detection
- [x] `Apple_HFS` partition discovery
- [x] HFS MDB parsing for volume name and sizing metadata
- [x] standalone regression coverage in `test_hfs.c`
- [x] catalog leaf-node scanning for the known catalog layout
- [x] direct extraction of root-level HFS data-fork files using first extent records
- [ ] full tree search using header and index nodes instead of the current leaf scan
- [ ] extents-overflow lookup beyond the first three extents

Scope:
- Apple Partition Map detection
- HFS Master Directory Block parsing
- catalog B-tree traversal
- data-fork extraction
- enough path handling to find `Descent`, `Install Descent`, and future direct
   HFS game assets

Structures to parse:
1. Apple Partition Map (APM): DDR at block 0 (sig `0x4552`), PM entries at
    blocks 1+ (sig `PM`), 512 bytes each, find `Apple_HFS`
2. HFS Master Directory Block (MDB): byte 1024 from partition start,
    sig `0x4244`
3. Catalog B-tree: node descriptors, header node, leaf nodes, thread records,
    file records, extents for data forks
4. Extents Overflow B-tree: probably unnecessary for the known pressed disc,
    but do not hardwire that assumption too deeply

Preferred API shape, more granular than the original rough sketch because it
supports easiest-first testing better:

```c
int hfs_find_partition(int bin_fd, int track_start_sector,
                                  int *hfs_start_sector_out, int *hfs_num_sectors_out);

int hfs_list_files(int bin_fd, int hfs_start_sector, int hfs_num_sectors,
                            hfs_file_list_t *out);

int hfs_extract_file(int bin_fd, int hfs_start_sector, int hfs_num_sectors,
                               const char *hfs_path, const char *output_path);

int hfs_extract_matching(int bin_fd, int hfs_start_sector, int hfs_num_sectors,
                                     const char **extensions, const char *output_dir,
                                     hfs_progress_fn progress, void *user_data);
```

References:
- `temp/parse_apm.ps1` for the already-proven APM algorithm
- Apple `Inside Macintosh: Files` for structure definitions
- `machfs` for high-level HFS catalog/B-tree behavior
- `hfsutils/libhfs` only if the public specs plus `machfs` leave holes

### B. STi2 extractor (scope-limited) -- hard but feasible

Implementation status after tranche 2:
- [x] STi2 archive recognition
- [x] CRC-validated entry-header scan over the real archive
- [x] parent-linked path reconstruction for listed entries
- [x] standalone regression coverage in `test_sti2.c`
- [x] method 13 decompression
- [x] output writing for matched files
- [x] exact-output regression coverage against the MacPlay oracle files

Scope:
- STi2 container parsing
- entry listing
- extraction of only the methods actually used by the known archive
- resource forks can be skipped for the Android use case

Known archive facts already established:
- archive magic: `STi2`
- declared header count: `13`
- confirmed visible entry-header offsets in the known archive:
   - `CHAOS.HOG` at `0x69398`
   - `CHAOS.MSN` at `0x7E595`
   - `descent.hog` at `0x7E767`
   - `descent.pig` at `0x456260`
   - `demo1.dem` at `0x5C0FC9`
   - `watchme.dem` at `0x62EC5A`
   - `yep9.dem` at `0x6837EB`
- the archive contains directory headers such as `Data` and `Demos`
- for this archive, entry listing is more reliable when driven by visible
   header scanning plus CRC validation and parent offsets than by assuming a
   purely sequential byte-22 walk
- likely compression set is small, with method 13 the main suspect
- verified compression set on the current regular-file outputs: method 13

Preferred API shape:

```c
int sti2_list_entries(const uint8_t *archive_data, size_t archive_size,
                                 sti2_entry_list_t *out);

int sti2_extract_entry(const uint8_t *archive_data, size_t archive_size,
                                  const sti2_entry_t *entry, const char *output_path);

int sti2_extract_matching(const uint8_t *archive_data, size_t archive_size,
                                       const char **extensions, const char *output_dir,
                                       sti2_progress_fn progress, void *user_data);
```

Why not use existing tools on Android:
- `stuffit-rs` targets classic `.sit`, not STi2 installer archives
- `unar`/XADMaster are not portable to the Android NDK stack used here
- bringing Rust into the Android app just for this would add friction the rest
   of the repo currently avoids

### C. JNI and launcher integration

Changes needed:
- `jni_disc_import.c`
   - add new Mac HFS/STi2 list/extract entry points
   - reuse the same progress callback style used by ISO/GOG importers
- `DiscImportBridge.kt`
   - add JNI declarations and Kotlin wrappers
- `SetupActivity.kt`
   - after ISO extraction fails, try Mac CD detection and Mac extraction
- `CMakeLists.txt`
   - add `hfs_reader.c` and `sti2_extract.c`

Current status after tranche 5:
- `jni_disc_import.c` now exposes `nativeExtractMacFiles(...)`
- `DiscImportBridge.kt` now wraps the native Mac extraction path alongside the
   existing ISO and SOW helpers
- `SetupActivity.kt` now tries the Mac HFS importer after ISO extraction
   returns no files for a data track
- The current native Mac import path first tries the proven
   `Install Descent` -> `STi2` route, then falls back to direct HFS
   extraction of matching game files if no installer archive is found
- Validation completed for this tranche:
   - `android/run-code-quality.ps1 --fix`
   - desktop extract rebuild plus full `ctest`
   - Android `:app:compileDebugKotlin :app:externalNativeBuildDebug`

## Easiest-to-hardest implementation order

The correct order here is to isolate one subsystem at a time. Do not start with
Kotlin or Android UI work. Get desktop-native extraction right first.

1. HFS partition detection only
    - Implement `hfs_find_partition()`
    - Validate against both known Mac D1 discs
    - Success criteria:
       - primary disc resolves as HFS, not ISO
       - secondary disc resolves too
   - Status:
      - done on 2026-04-12 via `hfs_reader.c` + `test_hfs.c`
      - `extract_cd.c` now reports HFS instead of surfacing a false ISO error

2. HFS catalog walk and direct-file extraction
    - Implement enough catalog/B-tree support to list files and extract data forks
    - First direct-HFS proof target on the primary MacPlay disc: `Install Descent`
    - Success criteria:
       - root-level `Install Descent` is found in the catalog listing
       - extracted `Install Descent` data fork starts with `STi2`
       - if a later disc stores final game files directly on HFS, reuse the same
          path for those assets before adding disc-specific shortcuts

3. STi2 entry parser with no decompression yet
    - Parse header and enumerate entries
    - Record entry name, output size, compression method, compressed offset
    - Success criteria:
       - known member offsets are confirmed for the primary archive
       - we know exactly which compression methods are actually present before
          writing a decompressor
   - Status:
      - done on 2026-04-12 via `sti2_extract.c` + `test_sti2.c`
      - current parser confirms regular MacPlay outputs and their archive header
         offsets, but does not yet decompress method 13 payloads

4. Stored entries first, if any
    - Implement method 0 only if the archive uses it
    - If nothing is stored, keep this phase as parser/assertion coverage and move on

5. Smallest compressed outputs first
    - `CHAOS.MSN` first, because it is tiny and Mac-specific
    - `CHAOS.HOG` second, because it is small and should match the existing D1
       v1.0 hash exactly
    - then the three Mac demo files
    - Success criteria:
       - `CHAOS.MSN` hash matches
          `be614df3ab4d35350f1033fc4acd7f18f779883273cdbf1db1d976df4be11d02`
       - `CHAOS.HOG` hash matches
          `0c5fb0684443c0b9c4dbb2769db65b8460bdad4ca80d58a943396fd07c001ba4`
   - Status:
      - done on 2026-04-12
      - `test_sti2` now extracts and byte-compares `CHAOS.MSN` and
         `CHAOS.HOG` along with the remaining regular outputs against the
         existing MacPlay oracle files

6. Large required members last
    - `descent.hog`
    - `descent.pig`
    - Success criteria:
       - `descent.hog` hash matches
          `d0ab72fda672ac9d751d24899f96ac14eb1720386cf2b0226a5add71543c4339`
       - `descent.pig` hash matches
          `9eb232c9da830309b2d3a1de75713f6155c62215cd867f4570536180af31f299`
   - Status:
      - done on 2026-04-12 for the primary MacPlay archive
      - the same `test_sti2` extraction regression now byte-compares
         `descent.hog`, `descent.pig`, `Descent`, and the three demo files too

7. JNI and Kotlin integration only after desktop-native success
    - wire `jni_disc_import.c`
    - add `DiscImportBridge.kt` wrappers
    - add Mac fallback in `SetupActivity.kt`
   - Status:
      - done on 2026-04-12
      - `nativeExtractMacFiles()` now reuses the existing JNI progress callback
         shape and extracts matching files from either the STi2 installer path
         or direct HFS files
      - the disc import dialog now falls back to the Mac path when ISO
         extraction returns no files on a data track

8. Regression coverage and emulator validation
    - create `game_data/CD images/Descent - Mac macplay/extract_regression.json5`
    - run `android/tests/test_extract.ps1` against that new spec
    - run an emulator import test using the Mac disc image
    - validate the imported D1 set launches and reaches `Lunar Outpost`
   - Status:
      - partially done on 2026-04-12
      - `game_data/CD images/Descent - Mac macplay/extract_regression.json5`
         now exists with the MacPlay source hashes and D1 full-game
         expectations
      - `android/tests/test_extract.ps1` now passes against that spec on the
         emulator and reaches `Lunar Outpost`
      - the current launcher import path extracts the 7 gameplay files and
         intentionally skips copying the downstream `Descent` executable
      - setup-command direct disc-import regression coverage can now exercise
         the launcher-side BIN/CUE import path from on-device file paths
      - remaining gap: this still does not cover the full SAF picker UI path
         from content URIs

9. Secondary-disc compatibility pass
    - rerun the same native code on `d1 mac 2nd bin+cue`
    - fix only if the second disc exposes a real compatibility issue rather than
       unnecessary over-generalization

## Planned regression spec details

When the native path is working, add:
`game_data/CD images/Descent - Mac macplay/extract_regression.json5`

Expected core fields:
- `source_type`: `cd`
- `source_files`:
   - cue SHA256 `b8127c0ce27a4573b596c5cfb78af0f0e7ce58e1dbe130e1424f3ede09d4446d`
   - bin SHA256 `38393a12630bbdfdd2e2efac138da463c242586a42c889b91054fabf6ba7b925`
- `game`: `d1`
- `classification`: `d1_full`
- `expected_mission`: `Descent: First Strike`
- `expected_level1`: `Lunar Outpost`
- `expected_files`: `descent.hog`, `descent.pig`
- `audio_tracks`: 13
- `total_extracted`: 8 if we keep `Descent` in the extracted set, otherwise 7

## AI-generated file attribution plan

The repo already has a precedent for this in `inno_reader.c` and `inno_reader.h`:
when one upstream codebase is the dominant inspiration, keep an explicit file
header that says so and include the upstream license notice.

### `hfs_reader.c` / `hfs_reader.h`

Current status after the first implementation tranche:
- the current `hfs_reader` code was written from public APM/HFS structure docs,
   the repo's existing notes, and the already-verified field offsets from the
   desktop investigation
- no single external codebase was directly followed closely enough in this
   tranche to justify a copied external license notice yet
- if the next tranche's catalog/B-tree code ends up leaning heavily on
   `machfs`, revisit this and add explicit MIT-style attribution at that time

Expected inspiration order:
1. public HFS/APM specs
2. `temp/parse_apm.ps1`
3. `machfs` (MIT)
4. `hfsutils/libhfs` (GPL-2.0) only if the earlier sources are not enough

Plan:
- Prefer the public specs plus `machfs` first, because that keeps the likely
   dominant inspiration MIT-licensed and the implementation simpler
- If the final code ends up following `machfs` structure closely, add a file
   header that explicitly cites `machfs` and keeps the MIT notice pattern
- If the final code instead leans heavily on `hfsutils/libhfs`, switch to a
   GPL-2.0-style attribution header for those files instead
- Do not silently mix multiple upstreams in one file without calling that out

### `sti2_extract.c` / `sti2_extract.h`

Expected inspiration order:
1. `XADMaster` STi2 and StuffIt code (LGPL-2.1)
2. `stuffit-rs` for method 13 ideas or tests (MIT or Apache-2.0)

Plan:
- The most likely dominant inspiration for the parser itself is `XADMaster`,
   because it is the only clearly relevant codebase here that actually knows
   about StuffIt-family parsing at the needed level
- If `XADMaster` is the dominant inspiration, add an LGPL-2.1 attribution
   header from the start, in the same spirit as the current `innoextract`
   attribution on `inno_reader.c`
- Actual landed state after method 13 implementation:
   - `XADMaster` is the dominant inspiration for the decompressor path
   - `sti2_extract.c` now carries an explicit provenance note for that source
- If method 13 decompression logic ends up taking more from `stuffit-rs` than
   from `XADMaster`, call that out explicitly and include the MIT/Apache notice
   that matches the final dominant source

### existing stuffit test files
- JSON-manifest tranche plan:
   - [x] choose a desktop-side expander utility path that can run on Windows and be pinned or clearly documented for manifest generation
   - [x] add a manifest-generation script that extracts each selected corpus archive with the desktop utility and writes committed JSON files with path, size, and SHA-256
   - [x] switch `test_stuffit_corpus.cpp` to load those JSON manifests and compare our listing or extraction results against them
   - [x] keep the current applicable subset explicit in the manifests so unsupported method 15 members stay documented but do not become false-positive extraction requirements
   - Current landed path:
      - manifest generation uses Windows `unar.exe` plus `lsar.exe` from The Unarchiver package, pinned by SHA-256 `61a6b299606282f72f51c278801eac11d3dccfac83e2d68bccce33539912e0dd`
      - `android/generate-stuffit-corpus-manifests.ps1` regenerates committed JSON fixtures under `android/app/src/main/cpp/extract/test_fixtures/stuffit_manifests/`
      - `test_stuffit_corpus.cpp` now compares our listing and supported data-fork extraction results against those committed manifests instead of the upstream repo `sources/` tree
- [x] inspect `https://github.com/ssokolow/stuffit-test-files`
- [x] pin the desktop-test fetch to commit `b523c64337867aee5cc07ad3a6bbfc8e06e4b3c0`
- [x] confirm the corpus is useful for classic `.sit` coverage, not direct `STi2`
- [x] identify the first applicable subset for the current minimal extractor:
   basic StuffIt 6.5/7 Mac `.sit` files with stored members `Test Image`,
   `testfile.jpg`, and `testfile.png`
- Verified nuance from the corpus tests:
   - the common stored subset across StuffIt 6.5 and 7 Mac archives is still
      `Test Image`, `testfile.jpg`, and `testfile.png`
   - StuffIt 6.5 Mac archives also store `Test Text` and `testfile.txt`
   - `testfile.PICT` remains method 15 in both versions, and the text files
      stay method 15 in the StuffIt 7 Mac archives
   - the first practical corpus method 13 sample is currently the old
      `testfile.stuffit45_dlx.mac9.sit` archive, so use that as a
      decompressor-only regression unless we later add native `SIT!` parsing
- [x] identify the first explicit non-applicable subset:
   Windows `.sit`, comment, password, receipt, `.sea`, `.exe`, `.sitx`, and
   old 4.5 `SIT!` files are out of scope for this tranche
- [x] wire the corpus into `android/app/src/main/cpp/extract/CMakeLists.txt`
   for desktop tests only
- [x] implement corpus tests that compare extracted supported files against
   committed external-tool JSON manifests instead of adding launcher/runtime code
- [x] extend beyond stored members using the first real corpus method 13 sample
   without adding unused method 15 support for current real media
   - audit result for current real inputs:
      - Mac BIN/CUE path still only needs method 13 for the proven regular-file
         outputs from `Install Descent`
      - current GOG installer paths are `pkg` and InnoSetup, not StuffIt
      - method 15 is therefore still not required for the currently supported
         real BIN/CUE or GOG import sources
   - landed follow-up:
      - `android/generate-stuffit-corpus-manifests.ps1` now records optional
         method and archive-offset metadata from `lsar -j`
      - `test_stuffit_corpus.cpp` now uses those committed manifest offsets to
         hash-check real classic method 13 data-fork members from
         `testfile.stuffit45_dlx.mac9.sit`

### Test tools and JNI glue

Expected inspiration:
- mostly repo-native patterns from `extract_cd.c`, `pkg_reader.c`,
   `jni_disc_import.c`, and the existing extract tests

Plan:
- no external attribution header unless a single external codebase becomes the
   dominant source for a specific file

### Lower-priority references

These are interesting but should not be treated as planned primary sources yet:
- `munbox`
- `peeler`

Reason:
- fit is not proven yet
- license handling has not been verified enough to plan around them today

## Risks and mitigations

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| STi2 format details are underdocumented | High | High | Do the entry-table-only phase first and decide compression support from real archive data, not guesses |
| Compression method assumptions are wrong | Medium | High | Confirm actual per-entry methods before writing the decompressor |
| HFS B-tree work grows past the expected simple case | Low | Medium | Keep the first pass read-only and disc-focused; only add overflow/extents complexity if the known discs force it |
| AI-generated code draws from mixed upstreams and attribution becomes unclear | Medium | Medium | Pick one dominant inspiration source per new file before coding and keep the header aligned with that choice |
| Multiple Mac disc variants differ more than expected | Medium | Medium | Treat the second disc as a follow-up compatibility pass after the primary disc works |
| Memory use on Android balloons if we copy the desktop approach too literally | Low | Medium | Keep the native path streaming and chunked; do not mirror the desktop Python in-memory behavior |

## Notes

- `CHAOS.HOG` is byte-identical to PC D1 v1.0 / GOG `CHAOS.HOG`
- `CHAOS.MSN` is not identical to the PC/GOG file and should be treated as its
   own Mac-specific known version
- `descent.hog` and `descent.pig` are Mac-specific versions and are already in
   `known_versions.json5`
- The existing desktop extractor is the oracle. The Android native path should
   not try to be cleverer than the desktop script until the hashes above match
- Redbook audio is not the blocker here. `known_discs.json5` and the track
   fingerprints already exist. First implementation only needs game-data import
