# Security scan: bad level and mod data

Date: 2026-10-15

## Executive summary

Malformed level or mod data can currently terminate the Android engine and, in several native parsers, can write or read outside fixed buffers. The highest-risk paths are compiled level geometry, level game-data records, the D2 POF loader, legacy PCX/IFF image decoders, and mission descriptor parsing. These are local-content attack surfaces: a user must import, install, or select attacker-controlled content, but no further trust should be assigned once that happens.

The scan found:

- 7 critical memory-safety fix areas
- 7 high-severity crash or resource-exhaustion fix areas
- 3 medium-severity hardening areas
- strong existing controls in Android archive extraction, KTX2 validation, the D1 POF parser, and several post-load texture/wall checks

This is a static security review, not a claim of code-execution exploitability. The critical rating means attacker-controlled data reaches unchecked native memory operations or fixed-array indexing. At minimum, the identified inputs can plausibly crash the engine; exploitability requires a separate analysis.

## Scope and threat model

The scan treats these inputs as hostile:

- `.msn` and `.mn2` mission descriptors
- `.rdl`, `.sdl`, `.rl2`, and `.sl2` level files
- HOG, DXA/ZIP, 7z, and RAR containers
- HAM, HXM/HX1, POG/PG1/DTX, PIG, and POF custom data
- PCX, IFF/BBM/ABM, PNG/JPG/TGA, and KTX2 replacement images
- launcher mod manifests, JSON patches, and song lists

The primary security properties are memory safety and availability. Secondary properties are bounded CPU, memory, and storage use, plus containment of extracted files beneath their intended root.

Third-party parser internals such as PhysicsFS archive backends, stb_image, libktx, SevenZipJBinding, and SDL_mixer were treated as dependencies and were not exhaustively audited. First-party validation around them was reviewed.

## Coverage ledger

The high-level pass inventoried format registration, import/staging, archive mounting, mission discovery, native level loading, custom-data loading, and image decoding. Modules selected for the deepest pass were read in contiguous approximately 500-line blocks through their final block.

| Area | Files and blocks completed | Result |
|---|---|---|
| Android archive budgets and streams | `ExtractionLimits.kt` 1-293; `ArchiveInputStreams.kt` 1-229; `ArchiveFiles.kt` 1-500, 501-563; `LauncherFileCopy.kt` 1-151 | Strong extraction budgets; uncapped source copies and host-tar link handling remain |
| Android mod and mission admission | `ModManager.kt` 1-500, 501-1000, 1001-1500, 1501-2000, 2001-2147; `MissionZip.kt` 1-500, 501-712; `MissionAssetCatalog.kt` 1-135; `DxaTextureScanner.kt` 1-296 | Several unbounded whole-entry reads and high archive cardinality limits |
| Descriptor policy and native mission loading | `GameFileFormats.kt` 1-500, 501-511; `MissionDescriptorPolicy.kt` 1-202; D1 `mission.c` 1-500, 501-966; D2 `mission.c` 1-500, 501-1000, 1001-1283 | Native parser has a null-pointer path and heap overwrite |
| Compiled level geometry | D1 `gamemine.c` 1-500, 501-643; D2 `gamemine.c` 1-500, 501-809 | Unchecked counts and references before geometry validation |
| Level game data | D1 `gamesave.c` 1-500, 501-1000, 1001-1500, 1501-1680; D2 `gamesave.c` 1-500, 501-1000, 1001-1500, 1501-1954; `level_section_io.h` 1-19 | Multiple fixed-array count and reference issues |
| Custom game data and models | D1 `custom.c` 1-500, 501-854; D2 `bm.c` 1-500, 501-682; D1 `polyobj.c` 1-500, 501-596; D2 `polyobj.c` 1-500, 501-874 | D1 POF path is hardened; D2 POF and HAM/HXM/POG paths are not |
| Legacy images and Android replacement textures | D1 `pcx.c` 1-443; D2 `pcx.c` 1-352; D1 `iff.c` 1-500, 501-1000, 1001-1017; D2 `iff.c` 1-500, 501-1000, 1001-1031; `pngfile.c` 1-500, 501-781 | PCX/IFF memory corruption; bounded KTX2; oversized compressed-image allocation |
| Shared native reads | D1 `physfsx.h` 1-374; D2 `physfsx.h` 1-369; D1/D2 `error.c` fatal path | Short reads terminate the engine instead of returning parse errors |

## Prioritized fix-area ledger

| ID | Severity | Area | Failure mode | Confidence | Proposed fix | Status |
|---|---|---|---|---|---|---|
| C-01 | Critical | Compiled level geometry | Vertex/segment counts can overrun global arrays | High | Runtime count and byte-span validation before any writes | Proposed |
| C-02 | Critical | Level game-data sections | Object/wall/trigger/matcen/light counts can overrun fixed arrays | High | Per-section count caps and checked span arithmetic | Proposed |
| C-03 | Critical | Level references and objects | Negative or oversized indices are dereferenced before validation | High | Decode to temporary state and validate the complete reference graph before publication | Proposed |
| C-04 | Critical | D2 POF models | Oversized files and unchecked cursor/chunk fields can corrupt stack or heap | High | Port the bounded D1 POF decoder or create a shared cursor parser | Proposed |
| C-05 | Critical | PCX images | RLE runs can overrun rows; dimensions can overflow allocation | High | Strict header validation and bounded scanline decoding | Proposed |
| C-06 | Critical | IFF/ABM images | Unchecked dimensions, chunks, palette size, RLE, and delta operations | High | Replace pointer decoder with checked cursor and row/plane budgets | Proposed |
| C-07 | Critical | Mission descriptors | Missing `=` can dereference address 1; suffix append can overwrite a 13-byte heap allocation | High | Safe token parser and one bounded filename-construction helper | Proposed |
| H-01 | High | D1 POG/PG1/DTX and HX1 | Counts, replacement indices, RLE lengths, and model sizes are unsafe | High | Full preflight plus transactional publication | Proposed |
| H-02 | High | D2 HAM/HXM | Table/model counts are not comprehensively bounded; malformed HXM calls fatal `Error()` | High | Central table validator and recoverable parse errors | Proposed |
| H-03 | High | Native scalar reads | Any truncated record using `PHYSFSX_read*` terminates the engine | High | Introduce checked readers that propagate parse failure | Proposed |
| H-04 | High | Level envelope | Signature is assertion-only, offsets are unchecked, and D2 flicker count is assertion-only | High | Validate signature/version/offsets/count before seeking or reading | Proposed |
| H-05 | High | Launcher metadata/patch parsing | Whole-entry reads and quadratic conflict comparison can OOM or stall | High | Bounded readers, JSON operation caps, and indexed conflict detection | Proposed |
| H-06 | High | SAF import copies | Unknown or lying provider sizes can fill app storage | High | Mandatory hard maximum on every external copy plus atomic staging | Proposed |
| H-07 | High | Host-tar RAR fallback | Listing validates names but not link targets; extraction can follow links | Medium | Reject links using structured metadata or remove the fallback | Proposed |
| M-01 | Medium | Replacement images | A compressed image can allocate 256 MiB before the 64 MiB decoded cap is checked | High | Lower source cap or stream stb_image input | Proposed |
| M-02 | Medium | DXA indexing | Admission allows 65,536 entries, 49,152 textures, and 16 MiB of paths | High | Align with the 4,096-entry extraction budget and add size/ratio budgets | Proposed |
| M-03 | Medium | Mod details UI | All ZIP entries are materialized in a list | High | Stream summaries and stop at the admission limit | Proposed |

## Detailed findings

### C-01: Compiled mine counts rely on debug assertions

D1 reads `Num_vertices` and `Num_segments`, then immediately loops into `Vertices[]` and `Segments[]`; the only upper-bound checks are `Assert` calls at `d1/main/gamemine.c:523` and `d1/main/gamemine.c:529`. D2 has the same pattern at `d2/main/gamemine.c:654` and `d2/main/gamemine.c:660`. Assertions are not an input-validation boundary in release builds. Old-format levels use 32-bit counts, so a large positive value writes beyond the fixed arrays. Signed negative counts also leave inconsistent global indices.

Fix: validate `1 <= Num_vertices <= MAX_VERTICES` and `1 <= Num_segments <= MAX_SEGMENTS` before loops. Compute the minimum serialized byte requirement with checked 64-bit arithmetic and reject impossible files before changing globals.

### C-02: Game-data section counts write past fixed tables

Both engines read record counts directly from the level header and use them as loop limits. Examples include objects, walls, triggers, and robot centers at `d1/main/gamesave.c:766-784` and `d2/main/gamesave.c:772-790`, plus D2 light tables at `d2/main/gamesave.c:795-798`. Object loops write `Objects[i]` at `d1/main/gamesave.c:832` and `d2/main/gamesave.c:850` without a `MAX_OBJECTS` check. Equivalent loops target `Walls`, `Triggers`, `RobotCenters`, `Dl_indices`, and `Delta_lights`.

`level_section_seek()` rejects negative counts and invalid starting offsets, but does not know a section's record size, maximum count, or required end offset. It therefore does not mitigate positive oversized counts.

Fix: define one descriptor per section containing maximum count and serialized record size for each supported version. Reject counts above `MAX_OBJECTS`, `MAX_WALLS`, `MAX_TRIGGERS`, `MAX_ROBOT_CENTERS`, `MAX_DL_INDICES`, and `MAX_DELTA_LIGHTS`, and prove `offset + count * record_size <= file_length` with overflow-safe arithmetic.

### C-03: References are used before a complete validation pass

Examples:

- Compiled segment child and vertex indices are accepted by `read_children()` and `read_verts()` and then consumed by `validate_segment_all()` without first proving they refer to loaded segments/vertices.
- Object segment validation checks only the upper bound at `d1/main/gamesave.c:994` and `d2/main/gamesave.c:1024`; a negative segment other than the sentinel can reach `obj_link()`.
- Non-robot polygon objects index `Save_pof_names[model_num]` before validating `model_num` at `d1/main/gamesave.c:272` and `d2/main/gamesave.c:240`.
- Trigger loops trust `num_links` and segment/side pairs. D1 dereferences `Segments[seg_num]` at `d1/main/gamesave.c:1077`; D2 derives a wall at `d2/main/gamesave.c:1111`. Neither path first proves `num_links <= MAX_WALLS_PER_LINK`, segment range, side range, and wall range.

Fix: parse into temporary storage, validate every index and sentinel, then commit globals. A single `validate_loaded_level_graph()` should cover segment children/vertices, objects, walls, triggers, matcens, control-center links, and light references before geometry, linking, rendering, or gameplay code runs.

### C-04: D2 POF decoding is memory-unsafe

`d2/main/polyobj.c:280` checks a model's file length only with `Assert`, then `d2/main/polyobj.c:283` reads the full attacker-controlled length into a 32,768-byte stack buffer. Release builds can therefore overflow the stack. Cursor helpers at lines 83, 115, and 128 read integers, shorts, and strings without checking `Pof_file_end`. Chunk lengths and `next_chunk` arithmetic are signed and unchecked. Submodel and gun counts/IDs are assertion-only, and `IDTA` allocates an unchecked signed length at line 417. `read_model_guns()` repeats the stack-length and cursor problems around lines 455-488.

D1 already uses `d1_pof_read()` with an explicit byte span and validates the source size before decoding. This is the preferred repair model.

Fix: port the D1 bounded decoder to D2, ideally as a shared first-party parser with explicit format limits. Do not attempt to patch only the initial stack read; all cursor, chunk, count, ID, and model-instruction validations must be part of the boundary.

### C-05: PCX RLE can overwrite bitmap rows

D1 and D2 derive dimensions from signed 16-bit coordinate differences without checking positivity, maximum dimensions, or multiplication overflow (`d1/2d/pcx.c:214-219`, `d2/2d/pcx.c:133-138`). During RLE decoding, a run count up to 63 is copied without clipping to the remaining row (`d1/2d/pcx.c:237-239`, `d2/2d/pcx.c:154-156`). A run near the row end therefore writes beyond the allocated scanline and possibly the bitmap. `BytesPerLine` is parsed but not used to constrain scanlines.

Fix: validate PCX coordinates, `BytesPerLine >= width`, plane count, and a conservative pixel budget. Decode exactly `BytesPerLine` bytes into a bounded row buffer, reject any run larger than the remaining encoded row, and copy only the visible width.

### C-06: IFF/ABM decoding has several independent overwrite paths

Both copies allocate `w * h` from signed 16-bit dimensions without checked multiplication (`d1/iff/iff.c:362`, `d2/iff/iff.c:356`). The BODY decoder forms `data_end` from unchecked width, height, and depth, then permits RLE `wid_cnt` to underflow before `memset` or copy (`d1/iff/iff.c:166,202-219`; D2 equivalents at 164, 200-217). DELTA operations advance or write before establishing that their repeat/skip/literal count fits the row (`d1/iff/iff.c:251-272`; D2 at 245-266). CMAP uses `len / 3` as a palette loop count with no 256-color cap (`d1/iff/iff.c:385-394`; D2 at 382-394). FORM/chunk end arithmetic is also unchecked.

These loaders are reachable through mission HOG overrides, briefings, exit assets, terrain assets, and ABM/BBM replacement resources.

Fix: use a size-aware cursor for every chunk, validate BMHD before allocation, cap planes and palette entries, and decode rows/deltas with explicit destination remaining counts. Reject duplicate or out-of-order chunks that require uninitialized state.

### C-07: Native mission descriptor parsing can crash or overwrite heap memory

`get_value()` does `strchr(buf, '=') + 1` before testing the result at `d1/main/mission.c:226` and `d2/main/mission.c:328`. A recognized directive without `=` produces undefined pointer arithmetic and normally dereferences address 1.

For `briefing` and `ending`, both engines allocate exactly `FILENAME_LEN` bytes, accept a value up to 12 characters, and call `strncat(tmp, ".tex", FILENAME_LEN)` or `.txb`. The append bound is the total buffer size rather than remaining capacity, so a 9-12 character stem without a dot overwrites the heap allocation. D1 sites are `d1/main/mission.c:655-687`; D2 sites are `d2/main/mission.c:923-955`.

The Kotlin descriptor policy ignores non-key lines and does not constrain asset-reference stems enough to make the native code safe, so launcher preflight is not a substitute.

Fix: make `get_value()` test the `strchr` result before incrementing. Normalize each directive with a shared parser, validate counts and filenames, and build suffixes with `snprintf` while checking its return value. Add native tests for missing `=`, empty values, maximum-length stems, truncated level lists, negative counts, and oversized counts.

### H-01: D1 custom texture and robot data lacks a complete preflight

`load_pog()` reads `num_bitmaps` and uses it in allocation and offset arithmetic without a range check at `d1/main/custom.c:236-248`. The normal load path does not use the safer preflight present in `piggy_read_level_bitmap_flags()`. Replacement indices are then used as `GameBitmaps[x]` at line 412 without proving they are in range. PPIG sets `no_repl` but leaves replacement indices unpublished/uninitialized for this path. RLE length `j` controls allocation and a four-byte header write without first proving `j >= 4` and within the file (`d1/main/custom.c:404-437`).

HX1 count loops and polymodel sizes are also attacker-controlled (`d1/main/custom.c:621-730`). A very large count runs until the shared scalar reader exits the process, while model sizes can drive large allocations and later interpretation of unvalidated model instructions.

Fix: reuse one mandatory POG/HX1 preflight for metadata and actual loading. Validate count, destination index, dimensions, checked pixel area, RLE length, file span, model metadata, and all dependent indices before replacing any global object. Publish replacements transactionally only after the whole file passes.

### H-02: D2 HAM/HXM table and model validation is incomplete

Base HAM loading reads many global counts and immediately fills fixed arrays, including `NumTextures`, robot/joint/model counts, and object bitmap counts at `d2/main/bm.c:175-219`. Extra HAM checks some upper bounds, but does not reject negative additions before pointer arithmetic and publication (`d2/main/bm.c:295-372`). HXM record counts are unbounded and invalid signatures, versions, or replacement IDs call fatal `Error()` (`d2/main/bm.c:380-448`). Polymodel disk records carry unchecked `model_data_size`, submodel pointers, texture ranges, and simpler-model references into `polygon_model_data_read()`.

Fix: introduce a shared table-count validator for base HAM, extra HAM, and HXM; reject both lower and upper bound violations. Validate the full replacement file in temporary objects, including model instruction spans, before freeing or replacing live models. Malformed optional content should return a recoverable error, not call `Error()`.

### H-03: Truncated native records deliberately terminate the process

`PHYSFSX_readInt`, `PHYSFSX_readShort`, and `PHYSFSX_readByte` call `Error()` on a short read in both engines (`d1/include/physfsx.h:268-300`, `d2/include/physfsx.h:268-298`). On Android, `Error()` reaches `android_fatal_error_exit()` (`d1/misc/error.c:82`, `d2/misc/error.c:82`). Thus a truncated level, HAM/HXM, POG, or other record using these helpers is a reliable engine-termination input even when it does not corrupt memory.

Fix: retain fatal readers only for trusted mandatory installation data if needed. Add `try_read_*` helpers for untrusted formats and propagate a structured parse error to the launcher/game UI. Avoid partially published globals on failure.

### H-04: Level envelope validation is assertion-only or ignored

Both `load_level()` implementations check the `PLVL` signature only with `Assert` (`d1/main/gamesave.c:1213`, `d2/main/gamesave.c:1277`). Mine and game offsets are passed to `PHYSFSX_fseek()` without checking its result. D2 reads `Num_flickering_lights` and protects the fixed array only with an assertion at `d2/main/gamesave.c:1309-1312`.

Fix: validate signature and supported version range at runtime; require offsets to be nonnegative, ordered when required, and within file length; check every seek; cap flickering lights before the loop. Return a level-load error without invoking the fatal boundary.

### H-05: Launcher patch and metadata parsing is unbounded

`ModManager` uses whole-entry reads for JSON patches/manifests and song lists at lines 1328, 1445, 1479, 1509, 1730, 1771, and 1959. A malicious ZIP entry can therefore allocate according to its expanded size. JSON is then materialized as an object tree. Conflict detection flattens all writes and compares every pair at `ModManager.kt:1393-1413`, yielding quadratic CPU and potentially quadratic diagnostic growth.

Fix: use a shared bounded text reader with separate limits for manifests, patches, and song lists. Reject too many JSON nodes, operations, writes, keys, nesting depth, and string bytes. Index writes by patch path and scope so conflict discovery is near-linear and cap emitted conflict details.

### H-06: External source copies have no mandatory maximum

`LauncherFileCopy.copyStream()` defaults `maxBytes` to `Long.MAX_VALUE` at `LauncherFileCopy.kt:25-30`. `ModManager.importMod()`, `importMissionZip()`, and nested ZIP extraction call it without a hard cap at lines 438, 532, and 660. Free-space preflight uses provider- or archive-declared sizes, which can be absent or false. The loop can consume the stream and fill storage before the final expected-size mismatch is detected.

Fix: require an explicit maximum for every external source. Use `MAX_ZIP_SOURCE_BYTES` for mission archives and define a documented DXA source cap. Write to an owned temporary file, enforce the limit while copying, fsync if required, validate, then atomically rename. Reserve space based on the hard cap when size is unknown.

### H-07: Host-tar fallback does not constrain link targets

The RAR fallback lists path strings with `tar -tf`, validates their projected names, and then runs `tar -xf` (`ArchiveFiles.kt:450-554`). A plain listing does not establish whether an entry is a symlink or hardlink or where its target points. `validateExtractedTree()` at lines 556-560 walks names and accounts sizes but does not reject links or prove each canonical node remains under the root. A crafted archive could create a link and cause a later archive member to write through it.

Fix: remove the host-tar fallback on Android, or use a structured listing that exposes entry type and link target, reject all links/devices, and extract entry-by-entry without following links. Canonical-check the parent immediately before each create/write and open files with no-follow semantics where available.

### M-01: Compressed replacement images can cause a large transient allocation

`read_png()` accepts a compressed source up to 256 MiB and allocates the entire file at `android/app/src/main/cpp/shared/pngfile.c:553-566`. The 64 MiB decoded-image cap is evaluated only afterward. On memory-constrained Android devices this can cause process OOM even for an image that will be rejected.

Fix: use stb_image callbacks over a bounded PhysFS stream, or reduce the source cap to a device-safe value consistent with the extraction memory budget. Maintain the existing dimension and decoded-byte checks.

### M-02: DXA cardinality budgets are much higher than general extraction budgets

`DxaTextureScanner` permits 65,536 entries, 49,152 texture entries, and 16 MiB of path strings (`DxaTextureScanner.kt:18-23`). The native texture index intentionally mirrors these values. This is bounded, but it is large enough to create noticeable CPU and heap pressure, and the scanner does not reject extreme compressed/uncompressed sizes or ratios.

Fix: align DXA admission with `ExtractionLimits.MAX_ENTRIES` unless real packages demonstrate a higher need. Add archive source, per-entry expanded-size, total expanded-size, and compression-ratio checks. Keep launcher and native constants synchronized in one documented contract.

### M-03: Mod detail inspection retains every entry

`getModDetails()` converts all non-directory ZIP entries to a list at `ModManager.kt:1092-1100`. With the current DXA entry cap, opening details for a hostile mod can retain tens of thousands of ZIP objects and derived document records.

Fix: stream counts/categories/documents, retain only a capped set needed for display, and report that the list was truncated.

## Existing defenses worth preserving

- `ExtractionLimits` provides central per-entry, total-expanded, entry-count, compression-ratio, descriptor, ZIP-source, and combined-memory budgets.
- ZIP staging validates central-directory and local-header relationships and limits preambles in `ArchiveInputStreams.kt`.
- Normal ZIP/7z extraction uses canonical output projection checks and accounts actual bytes while extracting.
- `MissionDescriptorPolicy` validates declared level counts, list completeness, filename length, and secret origins for launcher metadata.
- D1's standalone POF path validates file size and uses the bounded `d1_pof_read()` parser with negative tests.
- Android KTX2 loading caps source and decoded bytes and validates level offsets and exact ETC2 sizes before invoking libktx.
- Post-load level code repairs invalid texture references and rejects invalid segment wall references. These checks are useful but occur too late to protect earlier geometry/reference consumers.

## Recommended remediation order

1. Fix C-01 through C-07 before accepting arbitrary public level/mod uploads. These are native memory-safety boundaries.
2. Add a shared, recoverable native reader API and a staged level-validation pass. Use it for level, HAM/HXM, POG, and model formats.
3. Replace D2 POF parsing with the D1 bounded implementation and harden PCX/IFF with dedicated malformed corpus tests under ASan/UBSan.
4. Apply launcher source and text/JSON budgets, then reduce DXA entry limits and conflict-analysis complexity.
5. Remove or harden host-tar extraction.
6. Add a mutation corpus for each first-party format. The current mod fuzz campaign exercises combinations of valid packs and runtime transitions, not parser mutation. Seed the corpus with every ledger condition and require graceful rejection without `Error()`, signals, ANRs, or excessive resource use.

## Verification performed

- Read all selected modules through their final approximately 500-line block.
- Compared D1/D2 counterparts for mission, level, custom-data, model, PCX, and IFF paths.
- Confirmed Android `Error()` terminates the engine process.
- Ran `python android/tests/test_mod_fuzz_controls.py`: 13 tests passed.
- Ran `python android/tests/test_replacement_texture_limits.py`: passed.
- Ran `android/tests/test_bounded_extraction.ps1`: all traversal, alias, duplicate, reparse-point, and package extraction cases passed.

## Limitations

- No malicious proof-of-concept files were executed against the app in this pass.
- No complete third-party library audit was performed.
- ASan/UBSan mutation runs should be added as part of remediation; existing ASan build trees alone do not validate these cases.
- This report proposes fixes but intentionally does not change runtime behavior.
