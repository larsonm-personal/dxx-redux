# InnoSetup Binary Format Specification

## For versions 5.5.x / 5.6.x (Unicode builds)

*Derived from innoextract 1.9 source code. Focused on what's needed to implement a minimal C reader for GOG installers.*

---

## 1. Overall File Layout

An InnoSetup installer `.exe` contains:

```
[PE executable]
  ... at some offset ...
[Offset Table]           ← found via PE resource or fixed offset
  ... at header_offset ...
[setup-0.bin]            ← version ID + two block-compressed header streams
  ... at data_offset ...
[setup-1.bin]            ← compressed file data chunks
```

---

## 2. Offset Table (TSetupLdrOffsetTable)

### 2.1 Locating the Offset Table

**Method 1 (versions < 5.1.5):** Fixed offset at `0x30` in the EXE:
```
Offset 0x30:  uint32_t magic = 0x6F6E6E49   ("Inno" LE)
Offset 0x34:  uint32_t offset_table_offset
Offset 0x38:  uint32_t ~offset_table_offset  (bitwise NOT, for validation)
```

**Method 2 (versions >= 5.1.5):** PE resource lookup. The offset table is stored as PE resource type `RT_RCDATA` (type 10), name ID `11111`. Use standard PE resource parsing to find it.

### 2.2 Offset Table Structure

At the offset table position, the binary layout is:

```c
struct TSetupLdrOffsetTable {
    char     magic[12];        // Loader version magic (see below)
    uint32_t revision;         // = 1 (for versions >= 5.1.5)
    uint32_t exe_total_size;   // Total size (CRC-checked but not used for extraction)
    uint32_t exe_offset;       // Offset of setup.e32 (the installer code)
    // NOTE: exe_compressed_size is ABSENT for >= 4.1.6
    uint32_t exe_uncompressed_size;
    uint32_t exe_checksum;     // CRC32 of setup.e32
    uint32_t header_offset;    // *** Offset of setup-0.bin (headers) ***
    uint32_t data_offset;      // *** Offset of setup-1.bin (file data) ***
    uint32_t crc32;            // CRC32 of all preceding fields (for >= 4.0.10)
};
```

### 2.3 CRC32 Validation (>= 4.0.10)

A CRC32 is computed over all bytes from `magic[0]` through `data_offset` (inclusive). The stored CRC32 at the end must match. The 12-byte magic is included in the CRC.

### 2.4 Known Loader Magic IDs

For versions 5.1.5+, there are two known magic values (both map to version 5.1.5+):

| Magic bytes (12) | Version |
|---|---|
| `72 44 6C 50 74 53 CD E6 D7 7B 0B 2A` | >= 5.1.5 |
| `6E 53 35 57 37 64 54 83 AA 1B 0F 6A` | >= 5.1.5 |

---

## 3. Version ID String (setup::version)

At `header_offset`, the first 64 bytes are the version identifier string (null-padded):

```c
char version_string[64];  // e.g. "Inno Setup Setup Data (5.5.7) (u)"
```

The `(u)` suffix indicates **Unicode** build.

### 3.1 Relevant Version Strings

| String | Version Constant | Variant |
|---|---|---|
| `"Inno Setup Setup Data (5.5.0) (u)"` | 5.5.0.0 | Unicode |
| `"Inno Setup Setup Data (5.5.6) (u)"` | 5.5.6.0 | Unicode |
| `"Inno Setup Setup Data (5.5.7) (u)"` | 5.5.7.0 | Unicode |
| `"Inno Setup Setup Data (5.5.7) (U)"` | 5.5.7.0 | Unicode |
| `"Inno Setup Setup Data (5.5.8) (u)"` | 5.5.7.0 | Unicode (unofficial) |
| `"Inno Setup Setup Data (5.6.0) (u)"` | 5.6.0.0 | Unicode |
| `"Inno Setup Setup Data (5.6.2) (u)"` | 5.6.2.0 | Unicode (prerelease) |

**Ambiguity note:** Version 5.5.7 is ambiguous — it might actually be 5.5.7.1 or 5.6.0. innoextract tries all variants.

---

## 4. Block Stream Format (Header Compression)

Immediately after the 64-byte version string, the headers are stored as **two consecutive block streams**. The first contains the main header + all entry lists; the second contains data entries.

### 4.1 Block Stream Header

For versions >= 4.0.9 (which includes all 5.x):

```c
// Block stream header (9 bytes)
uint32_t header_crc32;      // CRC32 of the next 5 bytes
uint32_t stored_size;        // Total stored (compressed) size of the block data
uint8_t  compressed;         // 0 = Stored, 1 = compressed
```

If `compressed == 1`:
- For versions >= 4.1.6: compression = **LZMA1**
- For versions [4.0.9, 4.1.6): compression = **Zlib**

The CRC32 covers the 5 bytes: `stored_size` (4 bytes) + `compressed` (1 byte).

### 4.2 Block Data (4096-byte CRC Chunks)

The `stored_size` bytes of block data are divided into 4096-byte **sub-blocks**, each preceded by a CRC32:

```
For each sub-block:
    uint32_t chunk_crc32;     // CRC32 of the following chunk_data
    uint8_t  chunk_data[N];   // N = 4096, except last chunk may be shorter
```

The sub-block CRC protects the raw data. The total byte count of all `(4 + N)` pairs equals `stored_size`.

### 4.3 Decompression Pipeline

```
Raw bytes (stored_size) 
  → Split into CRC32-prefixed 4096-byte sub-blocks, validate each CRC
  → Concatenate validated sub-block data
  → Decompress with LZMA1 (for 5.x)
  → Decompressed header stream
```

### 4.4 LZMA1 Header Format (Inno-specific)

The LZMA1 stream used by Inno Setup is **NOT** standard LZMA Alone format. It differs:
- **5 bytes header only** (no 8-byte uncompressed size field)
- Byte layout:

```c
uint8_t  properties;      // Encodes lc, lp, pb:
                          //   pb = properties / (9 * 5)
                          //   lp = (properties % (9 * 5)) / 9
                          //   lc = properties % 9
uint32_t dict_size;       // Dictionary size (little-endian)
```

After these 5 bytes, the raw LZMA1 compressed stream follows (decoded with `LZMA_FILTER_LZMA1` in raw mode, no size limit).

---

## 5. Delphi String Serialization

All strings in the headers are serialized as **length-prefixed**:

```c
uint32_t length;          // String length in bytes (little-endian)
uint8_t  data[length];    // String data
```

For **Unicode** builds, strings are stored as UTF-16LE, so `length` is in bytes (2 bytes per character). They should be decoded from UTF-16LE.

Two string types in the code:
- **`binary_string`**: Raw length-prefixed, no encoding conversion
- **`encoded_string`**: Length-prefixed, then converted from UTF-16LE to UTF-8 (for Unicode builds)
- **`ansi_string`**: Length-prefixed, treated as Windows-1252

---

## 6. Stored Enums and Flags

### 6.1 Stored Enums

Enums are stored as a single `uint8_t` index into the enum values list.

### 6.2 Stored Flags (Bitfields)

Flags are stored as packed bitfields: **1 byte per 8 flags**.
- Exception: **exactly 3 bytes of flags are padded to 4 bytes** (for 32-bit builds, which all 5.x are).
- Bit 0 of byte 0 = first flag, bit 1 = second flag, etc.

### 6.3 Dynamic Flags (stored_flag_reader)

Some flag sets have a **variable number of flags** depending on version. These are read sequentially: each `add()` call reads the next bit from the bitfield, advancing through bytes as needed. After reading all flags for the version, if exactly 3 bytes were consumed, a 4th padding byte is read and discarded.

---

## 7. Header Structure (TSetupHeader)

The decompressed first block stream contains entries in this exact order:

### 7.1 Main Header Fields (for 5.5.x / 5.6.x Unicode)

```
Strings (all binary_string / encoded_string depending on field):
  app_name                          binary_string
  app_versioned_name                binary_string
  app_id                            binary_string
  app_copyright                     binary_string
  app_publisher                     binary_string
  app_publisher_url                 binary_string
  app_support_phone                 binary_string   (>= 5.1.13)
  app_support_url                   binary_string
  app_updates_url                   binary_string
  app_version                       binary_string
  default_dir_name                  binary_string
  default_group_name                binary_string
  base_filename                     binary_string
  uninstall_files_dir               binary_string
  uninstall_name                    binary_string
  uninstall_icon                    binary_string
  app_mutex                         binary_string
  default_user_name                 binary_string
  default_user_organisation         binary_string
  default_serial                    binary_string
  app_readme_file                   binary_string
  app_contact                       binary_string
  app_comments                      binary_string
  app_modify_path                   binary_string
  create_uninstall_registry_key     binary_string   (>= 5.3.8)
  uninstallable                     binary_string   (>= 5.3.10)
  close_applications_filter         binary_string   (>= 5.5.0)
  setup_mutex                       binary_string   (>= 5.5.6)
  changes_environment               binary_string   (>= 5.6.1)
  changes_associations              binary_string   (>= 5.6.1)
  license_text                      ansi_string     (>= 5.2.5)
  info_before                       ansi_string     (>= 5.2.5)
  info_after                        ansi_string     (>= 5.2.5)
  compiled_code                     binary_string   (>= 5.2.5)

Counts (all uint32_t little-endian):
  language_count
  message_count
  permission_count
  type_count
  component_count
  task_count
  directory_count
  file_count
  data_entry_count
  icon_count
  ini_entry_count
  registry_entry_count
  delete_entry_count
  uninstall_delete_entry_count
  run_entry_count
  uninstall_run_entry_count

Windows version range:
  winver_begin:  windows_version     (see §7.2)
  winver_end:    windows_version     (see §7.2)

Colors and misc:
  back_color                        uint32_t
  back_color2                       uint32_t
  image_back_color                  uint32_t        (< 5.5.7 ONLY, absent in >= 5.5.7)
  image_alpha_format                uint8_t enum    (>= 5.5.7: 0=Ignored, 1=Defined, 2=Premultiplied)

Password hash:
  password_sha1                     20 bytes        (>= 5.3.9)
  password_salt                     8 bytes         (>= 4.2.2)

Misc scalars:
  extra_disk_space_required         int64_t
  slices_per_disk                   uint32_t
  uninstall_log_mode                uint8_t enum
  privileges_required               uint8_t enum    (>= 5.3.7: 4 values)
  show_language_dialog              uint8_t enum
  language_detection                uint8_t enum
  compression                       uint8_t enum    (>= 5.3.9: 5 values - Stored/Zlib/BZip2/LZMA1/LZMA2)
  architectures_allowed             flags           (>= 5.6.0: 5 bits; < 5.6.0: 4 bits)
  architectures_installed_64bit     flags           (same)
  disable_dir_page                  uint8_t enum    (>= 5.3.3)
  disable_program_group_page        uint8_t enum    (>= 5.3.3)
  uninstall_display_size            uint64_t        (>= 5.5.0)

Header flags:
  (variable-length bitfield, see §7.3)
```

### 7.2 Windows Version

Each `windows_version` is 12 bytes (for >= 1.3.19):
```c
struct windows_version_data {
    uint16_t build;     // LE
    uint8_t  minor;
    uint8_t  major;
};  // 4 bytes

struct windows_version {
    windows_version_data win_version;   // 4 bytes
    windows_version_data nt_version;    // 4 bytes
    uint8_t  nt_sp_minor;              // 1 byte
    uint8_t  nt_sp_major;              // 1 byte
};  // 10 bytes total

struct windows_version_range {
    windows_version begin;  // 10 bytes
    windows_version end;    // 10 bytes
};  // 20 bytes total
```

### 7.3 Header Flags (5.5.x / 5.6.x)

The flags bitfield for 5.5.x/5.6.x contains these flags in order (each is one bit):

```
DisableStartupPrompt, CreateAppDir, AllowNoIcons, AlwaysRestart,
AlwaysUsePersonalGroup, WindowVisible, WindowShowCaption, WindowResizable,
WindowStartMaximized, EnableDirDoesntExistWarning, Password, AllowRootDirectory,
DisableFinishedPage, UsePreviousAppDir, BackColorHorizontal, UsePreviousGroup,
UpdateUninstallLogAppName, UsePreviousSetupType, DisableReadyMemo,
AlwaysShowComponentsList, FlatComponentsList, ShowComponentSizes, UsePreviousTasks,
DisableReadyPage, AlwaysShowDirOnReadyPage, AlwaysShowGroupOnReadyPage,
AllowUNCPath, UserInfoPage, UsePreviousUserInfo, UninstallRestartComputer,
RestartIfNeededByRun, ShowTasksTreeLines, AllowCancelDuringInstall,
WizardImageStretch, AppendDefaultDirName, AppendDefaultGroupName, EncryptionUsed,
ChangesEnvironment (< 5.6.1 only),
SetupLogging, SignedUninstaller, UsePreviousLanguage, DisableWelcomePage,
CloseApplications, RestartApplications, AllowNetworkDrive,
ForceCloseApplications (>= 5.5.7)
```

Total: ~44 flags → 6 bytes (no 3-byte padding issue).

### 7.4 Compression Method Enum (>= 5.3.9)

```
0 = Stored (no compression)
1 = Zlib
2 = BZip2
3 = LZMA1
4 = LZMA2
```

---

## 8. Entry Order in Header Stream

After the main header, entries are read in this exact order from the **same decompressed block stream**:

1. `language_count` × Language entries
2. `message_count` × Message entries
3. `permission_count` × Permission entries
4. `type_count` × Type entries
5. `component_count` × Component entries
6. `task_count` × Task entries
7. `directory_count` × Directory entries
8. **`file_count` × File entries** ← the ones we care about
9. `icon_count` × Icon entries
10. `ini_entry_count` × INI entries
11. `registry_entry_count` × Registry entries
12. `delete_entry_count` × Delete entries
13. `uninstall_delete_entry_count` × Uninstall-delete entries
14. `run_entry_count` × Run entries
15. `uninstall_run_entry_count` × Uninstall-run entries
16. **Wizard images** (>= 4.0.0): count=uint32_t (>= 5.6.0, else count=1), then count × binary_string
17. **Small wizard images**: same format
18. **Decompressor DLL**: binary_string (if compression is BZip2, or LZMA1 for 4.1.5, or Zlib >= 4.2.6)
19. **Decrypt DLL**: binary_string (if EncryptionUsed flag is set)

Then the first block stream ends. A **second block stream** follows (same format as §4), containing:

20. **`data_entry_count` × Data entries**

---

## 9. File Entry (TSetupFileEntry) — 5.5.x / 5.6.x Unicode

Each file entry is serialized as:

```
Strings:
  source                    encoded_string
  destination               encoded_string
  install_font_name         encoded_string
  strong_assembly_name      encoded_string       (>= 5.2.5)

Condition data (base item):
  components                encoded_string       (>= 2.0.0)
  tasks                     encoded_string       (>= 2.0.0)
  languages                 encoded_string       (>= 4.0.1)
  check                     encoded_string       (>= 4.0.0)
  after_install             encoded_string       (>= 4.1.0)
  before_install            encoded_string       (>= 4.1.0)

Windows version range:
  winver                    20 bytes             (see §7.2)

Scalars:
  location                  uint32_t             ← INDEX into data entry array
  attributes                uint32_t
  external_size             uint64_t             (>= 4.0.0)
  permission                int16_t              (>= 4.1.0)

Flags bitfield (variable, ~32 flags for 5.5.x):
  ConfirmOverwrite, NeverUninstall, RestartReplace, DeleteAfterInstall,
  RegisterServer, RegisterTypeLib, SharedFile, CompareTimeStamp,
  FontIsNotTrueType, SkipIfSourceDoesntExist, OverwriteReadOnly,
  OverwriteSameVersion, CustomDestName, OnlyIfDestFileExists, NoRegError,
  UninsRestartDelete, OnlyIfDoesntExist, IgnoreVersion, PromptIfOlder,
  DontCopy, UninsRemoveReadOnly, RecurseSubDirsExternal,
  ReplaceSameVersionIfContentsDiffer, DontVerifyChecksum,
  UninsNoSharedFilePrompt, CreateAllSubDirs, Bits32, Bits64,
  ExternalSizePreset, SetNtfsCompression, UnsetNtfsCompression,
  GacInstall
  → 32 flags = 4 bytes

File type:
  type                      uint8_t enum         (0=UserFile, 1=UninstExe)
```

The `location` field is the **index** into the data entries array (loaded from block stream 2). A location value of `0xFFFFFFFF` means no data.

---

## 10. Data Entry (TSetupDataEntry) — 5.5.x / 5.6.x

Each data entry describes one chunk of compressed data in setup-1.bin:

```c
struct TSetupDataEntry {
    uint32_t first_slice;         // Slice number (0-based for >= 4.0.0)
    uint32_t last_slice;          // Slice number
    uint32_t chunk_offset;        // Byte offset within the slice
    uint64_t file_offset;         // Offset of this file within the decompressed chunk (>= 4.0.1)
    uint64_t file_size;           // Decompressed file size
    uint64_t chunk_compressed_size; // Total compressed chunk size

    // Checksum of the decompressed file:
    uint8_t  sha1[20];           // SHA-1 hash (>= 5.3.9)

    // Timestamp:
    int64_t  timestamp;          // Win32 FILETIME format

    // File version:
    uint32_t file_version_ms;
    uint32_t file_version_ls;

    // Flags bitfield (variable):
    //   VersionInfoValid, VersionInfoNotValid,
    //   TimeStampInUTC (>= 4.0.10),
    //   IsUninstallerExe (>= 4.1.0),
    //   CallInstructionOptimized (>= 4.1.8),
    //   Touch (>= 4.2.0),
    //   ChunkEncrypted (>= 4.2.2),
    //   ChunkCompressed (>= 4.2.5),
    //   SolidBreak (>= 5.1.13),
    //   Sign (>= 5.5.7),
    //   SignOnce (>= 5.5.7)
    //   → For 5.5.7+: 11 flags → 2 bytes (no 3-byte padding issue)
};
```

### 10.1 Chunk Properties (Derived from Data Entry)

From the data entry flags and the main header:

- **Compression**: If `ChunkCompressed` flag is set, use `header.compression` (from §7.4). Otherwise `Stored`.
- **Encryption**: If `ChunkEncrypted` flag is set, use `ARC4_SHA1` (for >= 5.3.9). Otherwise `Plaintext`.
- **Filter** (exe instruction optimizer): If `CallInstructionOptimized` flag is set:
  - >= 5.3.9: `InstructionFilter5309`
  - [5.2.0, 5.3.9): `InstructionFilter5200`
  - < 5.2.0: `InstructionFilter4108`

### 10.2 Multiple Files Per Chunk (Solid Mode)

Multiple data entries can reference the **same chunk** (same first_slice + chunk_offset + chunk_compressed_size). Each file has a different `file_offset` within the decompressed chunk. A `SolidBreak` flag indicates the start of a new chunk boundary.

---

## 11. Chunk Data Format (in setup-1.bin)

### 11.1 Chunk Header

At the chunk's position in setup-1.bin (at `data_offset + chunk_offset`):

```c
char magic[4] = { 'z', 'l', 'b', 0x1A };  // Required magic for every chunk
```

After this 4-byte magic, the compressed data follows.

### 11.2 Encryption Layer (if encrypted)

If encryption is enabled, immediately after the magic:
```c
uint8_t salt[8];   // Random salt for this chunk
```

The decryption key is derived: `hash = SHA1(salt + password)`, then RC4 with 1000-byte discard.

### 11.3 Compression Layer

The data after magic (and after salt if encrypted) is the compression stream:

| Method | Stream Format |
|---|---|
| Stored | Raw bytes |
| Zlib | Standard zlib stream |
| BZip2 | Standard bzip2 stream |
| LZMA1 | 5-byte Inno header (see §4.4) + raw LZMA1 stream |
| LZMA2 | 1-byte dict prop + raw LZMA2 stream |

### 11.4 LZMA2 Header (1 byte)

```c
uint8_t prop;  // Dictionary size encoding
if (prop == 40) dict_size = 0xFFFFFFFF;
else dict_size = (2 | (prop & 1)) << (prop / 2 + 11);
```

### 11.5 File Extraction within a Chunk

After decompression, the chunk contains concatenated file data. Each file entry's `file_offset` and `file_size` (from the data entry) specify where within the decompressed stream to read.

If the `CallInstructionOptimized` filter is set, an additional exe-call instruction filter must be applied to the decompressed data before writing to the output file.

---

## 12. Complete Read Pipeline Summary

```
1. Open .exe, find offset table (PE resource 11111 or fixed offset 0x30)
2. Read header_offset and data_offset from offset table
3. Seek to header_offset
4. Read 64-byte version string → determine version & Unicode flag
5. Read block stream 1:
   a. Read 9-byte block header (CRC32 + stored_size + compressed_flag)
   b. Read stored_size bytes of CRC32-chunked data (4-byte CRC + up to 4096 bytes per sub-block)
   c. Decompress with LZMA1 if compressed
   d. Parse main header from decompressed stream
   e. Skip/parse all entry types through the stream (languages, files, etc.)
6. Read block stream 2 (same format):
   a. Parse data_entry_count data entries
7. For each file to extract:
   a. Look up file_entry.location → data_entry[location]
   b. Seek to data_offset + data_entry.chunk_offset in setup-1.bin
   c. Read 4-byte chunk magic
   d. Decompress chunk (LZMA1/LZMA2/Zlib/etc.)
   e. Seek to data_entry.file_offset within decompressed stream
   f. Read data_entry.file_size bytes
   g. Apply instruction filter if needed
   h. Verify SHA-1 checksum
```

---

## 13. Version-Specific Differences (5.5.3 → 5.5.7 → 5.6.2)

### 5.5.0 (baseline for 5.5.x)
- Added: `close_applications_filter` string in header
- Added: `uninstall_display_size` as uint64_t (was uint32_t in 5.3.6)
- Added flags: `CloseApplications`, `RestartApplications`, `AllowNetworkDrive`

### 5.5.6
- Added: `setup_mutex` string in header

### 5.5.7
- **Removed**: `image_back_color` uint32_t from header (no longer present!)
- Added: `image_alpha_format` uint8_t enum in header
- Added data entry flags: `Sign`, `SignOnce`  
- Added header flag: `ForceCloseApplications`

### 5.6.0
- Changed: architecture flags bitfield expanded from 4 entries to 5 (added ARM64)
- Changed: wizard images count is now `uint32_t` before each image set (instead of always 1)

### 5.6.1
- Added: `changes_environment` and `changes_associations` strings in header
- Removed: `ChangesAssociations` and `ChangesEnvironment` from header flags

### 5.6.2
- Same as 5.6.0 structurally (prerelease version, no format changes)

### Key Fields Present/Absent by Version

| Field | 5.5.0 | 5.5.6 | 5.5.7 | 5.6.0 | 5.6.2 |
|---|---|---|---|---|---|
| `close_applications_filter` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `setup_mutex` | ✗ | ✓ | ✓ | ✓ | ✓ |
| `changes_environment` | ✗ | ✗ | ✗ | ✗ | ✗ |
| `changes_associations` | ✗ | ✗ | ✗ | ✗ | ✗ |
| `image_back_color` in header | ✓ | ✓ | ✗ | ✗ | ✗ |
| `image_alpha_format` in header | ✗ | ✗ | ✓ | ✓ | ✓ |
| `ForceCloseApplications` flag | ✗ | ✗ | ✓ | ✓ | ✓ |
| `Sign`/`SignOnce` data flags | ✗ | ✗ | ✓ | ✓ | ✓ |
| wizard image count uint32_t | ✗ | ✗ | ✗ | ✓ | ✓ |
| architecture flags 5-bit | ✗ | ✗ | ✗ | ✓ | ✓ |

*(Note: `changes_environment` and `changes_associations` are only in >= 5.6.1, not 5.6.0)*

---

## 14. GOG-Specific Handling

### 14.1 No Format Differences

GOG installers use **standard InnoSetup format** — there are no GOG-specific magic values, offset table changes, or binary format modifications.

### 14.2 GOG Galaxy Multi-Part Files

GOG Galaxy uses InnoSetup's scripting system to split large files into parts:

- **`before_install`** script on the first file entry: `before_install('md5hash', 'filename', partcount)`
- **`after_install`** script on each part: `after_install('md5hash', 'compressed_size', 'uncompressed_size')`

The parts are individually zlib-compressed within the InnoSetup data. To reassemble:
1. Find the `before_install` call to get the output filename and part count
2. For each part, decompress the chunk data, then apply zlib decompression on the file data
3. Concatenate all parts in order

The extractor recognizes this convention only after parsing the complete call.
The function name is case-insensitive, the hash and destination arguments must
be single-quoted, doubled single quotes are decoded, the hash must contain
exactly 32 hexadecimal digits, the destination must be a valid bounded relative
path, and the unquoted part count must be decimal and between 1 and the
extraction entry limit. Any missing or extra argument, trailing statement,
invalid value, or oversized script remains ordinary opaque Inno
`BeforeInstall` metadata.

### 14.3 GOG Game ID & Password

- **Game ID**: Found in registry entries as `SOFTWARE\GOG.com\Games\{id}` with name `gameID`
- **Password** for encrypted installers: `lowercase_hex(MD5(game_id_string))`

### 14.4 GOG .bin Files

Some GOG installers ship with external `.bin` files (RAR archives). These are separate from the InnoSetup format and require unrar to process. They're identified by checking for accompanying `.bin` files matching the installer's base filename.

---

## 15. Skipping Entries You Don't Need

For a minimal extractor, you don't need to parse every entry type — but you **must read through them** because entries are serialized sequentially in the block stream with no random-access capability.

For entries you don't care about (languages, messages, components, etc.), you can skip them by reading and discarding the same fields. The key challenge is knowing the exact byte count for each entry type, which varies by version.

**Recommended approach**: Actually parse the `binary_string` length-prefix and skip that many bytes for each string field, and read+discard the fixed-size fields. This is safer than trying to compute total sizes.

---

## 16. Pseudocode for Minimal Reader

```c
// 1. Find offsets
offsets = find_offset_table(exe_file);
seek(offsets.header_offset);

// 2. Read version
char version_str[64];
read(version_str, 64);
parse_version(version_str);  // determine version number and Unicode flag

// 3. Read block stream 1
block1 = read_block_stream();  // CRC header + CRC-chunked + LZMA1 decompress

// 4. Parse main header from block1
header = parse_header(block1, version);
// Get file_count and data_entry_count

// 5. Skip entries we don't need, read file entries
skip_entries(block1, header.language_count, ...);
file_entries = parse_file_entries(block1, header.file_count);
skip_remaining_entries(block1, ...);

// 6. Read block stream 2
block2 = read_block_stream();

// 7. Parse data entries from block2
data_entries = parse_data_entries(block2, header.data_entry_count);

// 8. Extract files
for each file_entry:
    data = data_entries[file_entry.location];
    seek(offsets.data_offset + data.chunk_offset);
    read_chunk_magic();  // "zlb\x1a"
    decompress(data.chunk_compressed_size, header.compression);
    seek_in_decompressed(data.file_offset);
    write_output(data.file_size);
```
