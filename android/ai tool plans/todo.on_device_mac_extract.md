# On-Device Mac CD Extraction (Android)

Status: Future work -- not started

## Goal

Enable Android users to import game files directly from a Mac (HFS) CD image
on-device, without needing a PC extraction step. The Mac Descent CD (MacPlay)
uses an HFS filesystem containing a StuffIt Installer v2 (STi2) archive that
holds descent.hog, descent.pig, CHAOS.HOG, CHAOS.MSN, and demo files.

## Architecture

Follow the existing Android extraction pipeline pattern:

```
Kotlin SetupActivity
  -> DiscImportBridge.kt (new: nativeExtractMacCd)
    -> jni_disc_import.c
      -> cue_parser.c (existing -- CUE parsing, sector geometry)
      -> hfs_reader.c (NEW -- Apple Partition Map + HFS catalog reader)
      -> sti2_extract.c (NEW -- StuffIt Installer v2 decompressor)
```

## Components

### A. HFS Reader (hfs_reader.c / hfs_reader.h) -- FEASIBLE

~500-700 lines C. Parses Apple Partition Map, reads HFS Master Directory Block,
traverses Catalog B-tree, extracts data forks.

Structures to parse:
1. Apple Partition Map (APM): DDR at block 0 (sig 0x4552), PM entries at
   blocks 1+ (sig "PM"). Each 512 bytes. Find the "Apple_HFS" partition.
2. HFS Master Directory Block (MDB): At byte 1024 from partition start.
   Sig 0x4244. Contains allocation block size, catalog B-tree location.
3. Catalog B-tree: Standard B-tree with node descriptors, header node,
   leaf nodes containing file/folder thread records. File records have
   extent descriptors (start block + length) for the data fork.
4. Extent Overflow B-tree: Only needed for files with >3 extent runs
   (fragmented files). Unlikely needed for a pressed CD.

API (following iso9660_reader.h pattern):
```c
int hfs_find_partition(int bin_fd, int track_start_sector,
                       int *hfs_start_sector_out, int *hfs_num_sectors_out);

int hfs_list_files(int bin_fd, int hfs_start_sector, int hfs_num_sectors,
                   hfs_file_list_t *out);

int hfs_extract_files(int bin_fd, int hfs_start_sector, int hfs_num_sectors,
                      const hfs_file_list_t *file_list, const char *output_dir,
                      const char **extensions,
                      hfs_progress_fn progress, void *user_data);
```

Sector I/O reuses the Mode1/2352 raw sector access pattern from iso9660_reader.c.

References:
- temp/parse_apm.ps1 (APM algorithm, proven)
- Python machfs library (HFS reading reference)
- Apple "Inside Macintosh: Files" (public spec)
- libhfs source (~2000 LOC for full read-write HFS)

### B. STi2 Extractor (sti2_extract.c / sti2_extract.h) -- HARD BUT FEASIBLE

~500-800 lines C (scope-limited to known archive).

The Mac Descent installer uses StuffIt Installer v2 format:
- Magic: "STi2" at offset 0
- File entries at known offsets (descent.hog at 0x7E76A, descent.pig at 0x456263)
- Each entry: Pascal string name, file metadata, compressed data
- Compression: likely StuffIt method 13 (LZ77+Huffman)

Recommended approach:
1. Study XADMaster source (Objective-C, LGPL 2.1 -- license-compatible with GPL)
   for STi2 format specification
2. Implement only compression methods used by the known archive:
   - Method 13 (LZ77+Huffman) -- ~300 LOC
   - Method 0 (store) -- trivial
3. Skip resource forks (not needed for game data)

API:
```c
int sti2_extract(const uint8_t *archive_data, size_t archive_size,
                 const char *output_dir, const char **extensions,
                 sti2_progress_fn progress, void *user_data);
```

Why NOT use existing tools on Android:
- stuffit Rust crate: does NOT handle STi2 (only SIT 1.x/5.0) -- tested/confirmed
- unar/XADMaster: Objective-C + GNUstep -- not portable to Android NDK
- Rust->Android: no Rust toolchain in the project, high friction

### tricks
1. implement the extract code as a standalone utility for PC use, as was done with the PC CD extraction paths, and initially test with that
2. we have working extract code on PC, so we have known-good extracted files. leverage that to check a re-implemented extract utility
3. decompiling the mac binary may give some insights

### existing implementations
1. there are minimal test stuffit files here: https://github.com/ssokolow/stuffit-test-files
2. a C expander library that might be useful: https://github.com/idolpx/munbox/tree/main/lib/layers
3. objective C library: https://github.com/MacPaw/XADMaster
4. rust library: https://github.com/benletchford/stuffit-rs
5. slop library in C: https://github.com/pappadf/peeler

### C. JNI Integration

Changes needed:
- jni_disc_import.c: new nativeExtractMacCd function
  Flow: parse CUE -> find data track -> hfs_find_partition -> hfs_extract_files
  (extract STi2 to temp) -> sti2_extract -> copy game files
- DiscImportBridge.kt: new JNI declaration + wrapper
- SetupActivity.kt: after ISO extraction fails, try Mac CD extraction
  (detect Apple DDR signature 0x4552)
- CMakeLists.txt: add extract/hfs_reader.c and extract/sti2_extract.c

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| STi2 format underdocumented | HIGH | HIGH | Study XADMaster source (LGPL) |
| Compression method unknown | MEDIUM | HIGH | Method 13 likely; stuffit-rs has reference impl |
| HFS B-tree complexity | LOW | MEDIUM | Pressed CDs: simple, unfragmented layout |
| Memory on Android | LOW | LOW | Stream-based, same as ISO flow |
| Multiple Mac CD variants | MEDIUM | MEDIUM | Design API generically, test with known disc |

## Implementation Phases

### Phase A -- HFS Reader (independent, lower risk)
1. Write hfs_reader.c / hfs_reader.h in android/app/src/main/cpp/extract/
2. Desktop test: test_hfs.c against Mac macplay bin
3. JNI wrapper for hfs_find_partition + hfs_extract_files

### Phase B -- STi2 Extractor (needs format research first)
1. Study XADMaster source for STi2 format specification
2. get minimal test archives from the repo for that
3. study other repos for context, but most are not able to do sti2 (see repos section)
4. Analyze "Install Descent" binary (identify compression methods per entry)
5. Write sti2_extract.c / sti2_extract.h
6. test with minimal test archives
7. Desktop test against "Install Descent" archive
8. JNI wrapper

### Phase C -- End-to-end (depends on A + B)
1. Wire nativeExtractMacCd in jni_disc_import.c
2. Kotlin bridge + SetupActivity UI flow
3. Android emulator test with Mac CD image

## Pre-requisite

Phase B requires a focused research session analyzing:
- XADMaster's XADStuffItParser.m (format parsing)
- XADMaster's compression method implementations
- The "Install Descent" binary itself (to identify exact methods per entry)

This research should happen before committing to the full C implementation.

## Notes

- CHAOS.HOG (Mac) is byte-identical to chaos.hog (D1 v1.0 PC) -- same SHA-256
- CHAOS.MSN differs between Mac and PC but .msn not in hash extensions list
- descent.hog and descent.pig are Mac-specific (unique hashes)
- The desktop extraction pipeline (extract_mac_cd.ps1) uses Python machfs + unar
  as a working reference implementation
