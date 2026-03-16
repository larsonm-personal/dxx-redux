# Mac .pkg Parser Implementation Plan

## Format Chain
Mac GOG installers (.pkg) use a three-layer format:
1. **XAR** — outer container (magic `xar!`, 28-byte BE header + zlib-compressed TOC XML + data heap)
2. **gzip** — the `package.pkg/Scripts` entry in the XAR is a gzip-compressed cpio archive
3. **cpio "odc"** — old character format (magic `070707`, 76-byte ASCII headers)
   - Game files are at `./payload/Contents/Resources/game/*`

## Key Format Details

### XAR Header (28 bytes, big-endian)
| Field | Size | Value |
|---|---|---|
| magic | u32 | `0x78617221` ("xar!") |
| header_size | u16 | 28 |
| version | u16 | 1 |
| toc_compressed_size | u64 | varies |
| toc_uncompressed_size | u64 | varies |
| checksum_algo | u32 | 1 (SHA1) |

- TOC is zlib-compressed (standard zlib format, not raw deflate)
- Heap starts at `header_size + toc_compressed_size`
- Data offsets in TOC are relative to the heap start

### TOC XML Structure
```xml
<xar><toc>
  <file id="1">
    <type>directory</type>
    <name>package.pkg</name>
    <file id="2">
      <data>
        <encoding style="application/octet-stream"/>
        <offset>9492</offset>
        <size>22235116</size>         <!-- uncompressed -->
        <length>22235116</length>     <!-- stored in heap -->
      </data>
      <type>file</type>
      <name>Scripts</name>
    </file>
  </file>
</toc></xar>
```

### cpio odc header (76 bytes, ASCII octal)
| Field | Offset | Length |
|---|---|---|
| c_magic | 0 | 6 ("070707") |
| c_dev | 6 | 6 |
| c_ino | 12 | 6 |
| c_mode | 18 | 6 |
| c_uid | 24 | 6 |
| c_gid | 30 | 6 |
| c_nlink | 36 | 6 |
| c_rdev | 42 | 6 |
| c_mtime | 48 | 11 |
| c_namesize | 59 | 6 (includes NUL) |
| c_filesize | 65 | 11 |

No alignment padding (unlike "newc" format).
Last entry has name `TRAILER!!!` and filesize 0.

## Verified .pkg Contents

### D1 (`descent_enUS_1_0_35122.pkg`, 22,251,081 bytes)
- XAR: TOC compressed=4377, uncompressed=13596, heap_offset=4405
- Scripts: offset=9492, length=22235116 (octet-stream=uncompressed)
- Scripts is gzip-compressed → decompresses to 39,505,408 bytes of cpio
- Game files at `./payload/Contents/Resources/game/`:
  DESCENT.HOG (6,856,701), DESCENT.PIG (4,920,305), plus misc

### D2 (`descent_2_enUS_1_0_51877.pkg`, 587,449,987 bytes)
- XAR: TOC compressed=4374, uncompressed=13600, heap_offset=4402
- Scripts: offset=9492, length=587435319 (octet-stream)
- Same structure, game files under `./payload/Contents/Resources/game/`

## Implementation

### Consolidated into `pkg_reader.h/c`
XAR and cpio parsers are internal implementation details since neither has
other users. This minimizes file count while keeping the public API clean.

### API
```c
int pkg_open(const char *pkg_path, pkg_archive_t *arc);
int pkg_extract_all(pkg_archive_t *arc, const char *output_dir,
                    pkg_progress_fn progress, void *user_data);
void pkg_close(pkg_archive_t *arc);
```

### Streaming Strategy
D2's gzip payload is 587MB — can't load into memory. Pipeline:
1. Parse XAR header + TOC (small, in memory)
2. Find Scripts entry → absolute file offset
3. Seek to Scripts, open zlib inflate (gzip mode: MAX_WBITS + 16)
4. Stream cpio entries through inflate: read header → check name → extract or skip data
5. Two passes: pkg_open (scan only), pkg_extract_all (extract matching files)

### File Matching
Extract files from `./payload/Contents/Resources/game/` matching:
`.hog .pig .ham .s11 .s22 .dem .mvl .msn .mn2 .gog .inst`
(Same extensions as extract_gog.c's existing filter)

## Files
- New: `pkg_reader.h`, `pkg_reader.c`
- Modified: `extract_gog.c`, `CMakeLists.txt`, `GOG_EXTRACTION_PLAN.md`
