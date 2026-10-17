# Bad level and mod data fixes

## Goal

Begin remediating the highest-risk findings from `android/security_scan_bad_level_mod_data_20261015.md`, preserving D1/D2 behavior for valid levels while rejecting hostile counts and references before they can write outside fixed arrays.

## Initial tranche

1. Add release-build validation for compiled mine vertex and segment counts in D1 and D2
2. Add release-build limits for level game-data section counts before section readers write global arrays
3. Harden immediately consumed object, model, and trigger references where a narrow validation can be applied without redesigning the loader
4. Add focused malformed-level regression coverage
5. Format changed files and run the relevant host tests/builds
6. Update the security ledger with implemented and remaining work

## Progress

- [x] Re-read repository guidance and the security ledger
- [x] Trace loader return conventions and existing validation helpers
- [x] Implement the first native validation tranche
- [x] Add regression coverage
- [x] Run formatting, focused tests, and build verification
- [x] Update the fix ledger and record follow-up work

## Initial tranche result

- D1 and D2 compiled-mine loaders now reject zero, negative, or oversized vertex and segment counts before writing global arrays
- D1 and D2 reject invalid segment child and vertex references before geometry validation dereferences them
- Game-data object, wall, trigger, matcen, and D2 lighting counts are capped at their destination array sizes
- Trigger segment/side links are validated before later fix-up code dereferences them
- Negative object segment references are discarded instead of being passed to `obj_link()`
- Shared boundary tests cover count, geometry-reference, trigger-link, offset, and section-limit rejection

Remaining work in these areas includes exact serialized section-span validation, checked scalar reads, object model-index validation, and transactional publication of decoded level state.

## Verification

- Scoped code-quality formatting passed for the shared helper and focused test
- Full Windows `RelWithDebInfo` builds passed for D1 and D2
- `test_level_section_io` passed for the D1 and D2 build trees
- Host metadata generation passed for built-in First Strike, built-in Counterstrike, and the external `trainng.zip` mission archive

## Sequential remediation

### C-01 compiled level geometry - complete

- Counts are decoded into locals and bounded before publication
- Checked 64-bit arithmetic proves the remaining file contains all vertices and the fixed minimum bytes for every segment before geometry-array writes begin
- Minimum segment sizes cover old/new encodings, compressed static light, and trailing `segment2` records
- Boundary tests cover old/new format sizes, truncation, negative counts, multiplication/addition overflow, and exact-fit spans
- Full D1/D2 Windows builds, both focused tests, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change

### C-02 level game-data sections - complete

- Every fixed-array count remains capped before its decode loop
- Checked 64-bit section arithmetic rejects multiplication overflow, invalid offsets, and truncated section ends before seeking
- Wall, trigger, reactor-link, matcen, and D2 light tables use exact version-specific serialized record sizes
- Variable-size object records require their fixed 79-byte base span for every declared object; checked type-dependent reads remain tracked by H-03
- Boundary tests cover exact-fit spans, one-record truncation, multiplication overflow, invalid offsets, and invalid counts
- Full D1/D2 Windows builds, both focused tests, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change

### C-03 level references and objects - complete

- Raw saved polymodel indices are no longer normalized with unsafe modulo arithmetic and must be in the saved-name table before lookup
- Remapped polygon models must be valid current model indices before later rendering code can consume them
- A pre-consumer graph pass validates object segments, wall locations and links, matcen references, segment wall references, trigger links, and D2 static/delta light ranges
- Optional `-1` sentinels and checked index ranges are centralized in the shared level boundary helper and covered by boundary tests
- Full D1/D2 Windows builds, both focused tests, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change

### C-04 D2 POF models - complete

- D2 now reads POF files only after proving the file fits the fixed input buffer and the complete read succeeds
- The shared bounded POF decoder validates headers, versions, chunks, counts, IDs, parents, instruction offsets, and animation/gun tables without unchecked pointer or signed chunk arithmetic
- Every submodel instruction tree, computed model bounds, texture reference, and robot-joint allocation is validated before engine tables consume the model
- Regression coverage composes the bounded chunk decoder with the recursive instruction validator and rejects unknown opcodes and truncated IDTA data
- Full D2 Windows build, both focused POF/validation tests, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change
- Converting fatal resource parse errors into recoverable load failures remains tracked by H-03

Next: harden PCX dimension, allocation, and RLE row handling for C-05.

### C-05 PCX images - complete

- Both engines reject reversed, zero, oversized, or excessive decoded dimensions before bitmap allocation
- `BytesPerLine` must cover the visible width and remain within the dimension and 64 MiB decoded-image budgets
- RLE decoding is row-buffered and rejects zero or overlong runs before any destination write; encoded padding is consumed but not copied into visible pixels
- Caller-owned bitmaps must prove width, height, data, and linear row capacity before decoding
- Failed decodes release loader-owned bitmap storage instead of publishing partial pixels
- D1's separate obfuscated `btexture.xxx` path now bounds file size, performs its reversal in one allocation, uses a checked memory cursor, and validates its optional palette
- Regression coverage exercises valid row padding, zero/overlong runs, undersized rows and destinations, reversed/oversized coordinates, and the obfuscated loader
- Full D1/D2 Windows builds, both upstream compatibility suites, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change

### C-06 IFF/ABM images - complete

- Both engines route public bitmap, caller-owned bitmap, and animation entry points through one shared size-aware decoder
- FORM and chunk lengths, alignment, BMHD dimensions and planes, palette entries, decoded image bytes, animation frames, and aggregate allocation are bounded before publication
- Raw and ByteRun1 PBM rows, planar ILBM rows and masks, and animation DELTA skip/repeat/literal operations use explicit source and destination budgets
- Failed bitmap and animation decodes release temporary storage instead of publishing partial images or frames
- Focused malformed coverage exercises truncation, forged chunks, dimensions, palettes, row overruns, planar data, delta operations, and animation frame capacity
- Three small representative malformed images are centralized and documented under `android/tests/fixtures/bad_test_inputs`; generated compatibility artifacts now stay in build-local test directories
- Full D1/D2 Windows builds, focused decoder tests, both upstream compatibility suites, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change

Next: replace unsafe mission descriptor value and filename handling for C-07.

### C-07 mission descriptors - complete

- D1 and D2 value parsing checks for `=` before pointer advancement and treats empty values as invalid
- Briefing and ending filenames are built in a fixed stack buffer only after proving the stripped stem plus `.tex` or `.txb` fits; path separators are rejected
- Level and secret counts use checked numeric conversion with explicit lower and upper bounds instead of `atoi`, assertions, or clamping
- Declared level and secret lists reject EOF, empty/oversized names, missing commas, and secret origins outside the loaded normal-level range
- D2 alternate-HAM values are tokenized and validated before allocation or publication
- Four small malformed mission descriptors join the centralized documented corpus under `android/tests/fixtures/bad_test_inputs`
- Focused tests, full D1/D2 Windows builds, both upstream compatibility suites, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change

Next: preflight and transactionally publish D1 POG/PG1/DTX and HX1 custom data for H-01.

### H-01 D1 POG/PG1/DTX and HX1 - complete

- Added a shared whole-buffer validator with 64 MiB per-file/image limits and a 128 MiB aggregate payload budget
- PIG, PPIG, DPOG, and DTX validation now bounds directory counts, replacement lookups, dimensions, offsets, raw spans, sound spans, and complete small/large RLE streams before allocation or publication
- HX1 validation now bounds every table count and destination index, robot joint spans, model sizes, submodel entry points, texture ranges, wreck references, object bitmaps, recursive model instruction branches, and exact file termination
- Native D1 preflights the complete PG1/DTX/HX1 set before removing active replacements; an invalid member leaves the current generation intact and an unexpected late apply failure restores the base generation
- PPIG name-based replacement indices are initialized, large-RLE flags are preserved, per-read spans remain checked after preflight, and HX1 restore pointers are cleared after ownership is returned
- Focused validator tests cover valid minimal DPOG/HX1 data plus oversized counts, invalid indices and offsets, malformed model instructions, and truncation; the native integration test additionally proves malformed HX1/PG1/DTX files cannot alter active replacements
- Scoped code quality, full D1/D2 Windows builds, focused tests in both build trees, both upstream compatibility suites, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change

Next: bound and make recoverable the D2 HAM/HXM table and model loaders for H-02.

### H-02 D2 HAM/HXM tables and models - complete

- Added a D2-local 64 MiB whole-buffer validator for base HAM versions 1-3, versioned extra HAM, and per-level HXM files
- Every fixed-array table count is checked for both lower and upper bounds before the engine readers run; extra additions are checked against their release-table bases without signed pointer arithmetic
- Robot model references, gun/submodel limits, animation joint spans, joint submodels, polymodel sizes, submodel entry points, local texture references, simpler models, dying/dead models, object bitmaps, and object bitmap pointers are validated before allocation or replacement
- Model payloads are capped at 16 MiB each and 64 MiB total and every active submodel instruction stream is walked with the existing bounded opcode validator
- Main HAM validation occurs before mission reload frees the current model set; malformed extra HAM and HXM files return recoverably before freeing or replacing live definitions, and optional loader ID/index checks no longer call fatal `Error()`
- Focused tests cover bounded empty files, truncation, negative and oversized counts, and invalid versions; integration coverage corrupts table counts, model sizes, instructions, signatures, and destination indices while asserting the active robot/model generation remains unchanged
- Scoped code quality, full D1/D2 Windows builds, the focused D2 validator, both upstream compatibility suites, built-in First Strike/Counterstrike, and `trainng.zip` passed after the change

Next: replace fatal native scalar-read behavior at attacker-controlled parse boundaries for H-03.

### H-03 recoverable native scalar reads - complete

- Added an opt-in shared checked-reader state for byte, short, integer, fixed-point, vector, angle-vector, and matrix fields without changing legacy mandatory-resource reader semantics
- D1 and D2 level envelopes, variable object records, direct trigger records, and compiled-mine streams now use sticky checked reads and return their existing load-error codes before failed records reach validation or gameplay
- Raw saved-model-name table reads now require an exact record count; fixed-size wall, trigger, reactor, matcen, and lighting readers remain protected by the exact section-span preflights added in C-02
- HAM/HXM and D1 POG/PG1/DTX/HX1 scalar reads remain safe behind the complete preflights added in H-01 and H-02
- Focused tests prove exact reads preserve values and truncated compound reads set sticky failure without invoking `Error()`; full D1/D2 builds and both upstream compatibility suites passed

Next: validate complete level envelopes, offsets, and D2 flickering-light counts for H-04.

### H-04 level envelope - complete

- D1 and D2 now reject invalid level signatures, versions outside the supported 1-8 range, negative section offsets, and offsets at or beyond the physical file length before parsing sections
- Mine and game section seeks are checked and return the existing recoverable load-error codes instead of continuing from a failed seek
- D2 bounds the flickering-light count to the destination capacity and preflights the full 16-byte-per-record serialized span before invoking the legacy record reader
- Focused tests cover valid envelopes plus bad signatures, versions, negative offsets, and end-of-file offsets; full D1/D2 builds and both upstream compatibility suites passed

Next: bound launcher metadata and patch parsing and replace quadratic conflict detection for H-05.

### H-05 launcher metadata and patch parsing - complete

- Added shared bounded UTF-8 readers with separate 256 KiB manifest/song-list limits and a 4 MiB patch limit; ZIP-declared sizes are prechecked and streaming reads enforce the limit when sizes are unknown or false
- JSON is lexically preflighted before `JSONObject`/`JSONArray` materialization, then structurally revalidated, with caps on depth, nodes, keys, and aggregate decoded string bytes
- Patch documents are capped at 4,096 operations and 8,192 writes, with 64 documents per mod and 128 across an enabled set; malformed or over-budget metadata now produces a fail-closed compatibility problem
- Conflict discovery indexes writes by patch path, row, field, value, and owning mod, replacing the pairwise comparison; each conflict report emits at most 64 detailed paths plus a truncation marker
- Focused bounded-input and ModManager integration tests, scoped Kotlin quality checks, and the complete 1,180-case Android JVM suite passed (one existing fixture-dependent skip)

Next: impose mandatory source-copy limits and atomic staging for SAF imports in H-06.

### H-06 SAF and external source copies - complete

- Removed unbounded `Long.MAX_VALUE` defaults from stream, URI, and caller-supplied input copy paths; every external caller now supplies a finite policy limit, while owned file-to-file copies default to the captured source length
- Added a documented 2 GiB DXA source ceiling alongside the existing 2 GiB mission-archive ceiling, enforced both against declared sizes and during streaming when a provider omits or lies about size
- Unknown-size copies reserve storage against the complete hard limit plus existing headroom instead of reserving zero bytes
- DXA, mission archive, and preferred nested mission imports now copy to fsynced owned staging files, validate before atomic publication, retain format extensions needed by ZIP/7z/RAR readers, and preserve an existing destination when validation fails
- Focused copy and ZIP/7z/RAR mission-import tests, scoped Kotlin quality checks, and the complete 1,182-case Android JVM suite passed (one existing fixture-dependent skip)

Next: remove or harden the host-tar RAR extraction fallback for H-07.

### H-07 host-tar RAR fallback - complete

- Removed the process-based `tar -tf`/`tar -xf` fallback, its name-only listing validation, diagnostic files, polling loop, and post-extraction tree walk
- RAR extraction now requires the structured SevenZipJBinding catalog and writes each admitted non-directory entry through an explicit `FileOutputStream`; archive link entries cannot cause filesystem links to be created
- Native-backend absence and unsupported RAR capability now delete partial output and fail closed instead of invoking an extractor that could follow symlink or hardlink targets
- Focused policy tests cover unavailable, unsupported, linkage, integrity, cancellation, catalog, and output-policy failures; the real RAR fixture either uses the structured backend or verifies the explicit fail-closed result
- Scoped Kotlin quality checks and the complete 1,182-case Android JVM suite passed (one existing fixture-dependent skip)

Next: reduce the transient compressed-image allocation before PNG decoding for M-01.

### M-01 replacement-image source allocation - complete

- Reduced the stb_image replacement source ceiling from 256 MiB to 64 MiB, matching the existing maximum decoded-pixel budget
- The source length is rejected before allocation and remains exact-read checked, preventing a malformed replacement from obtaining the former 256 MiB transient buffer before header/dimension validation
- Scoped C quality checks and the complete Android native D1/D2 debug build passed for arm64-v8a, armeabi-v7a, and x86_64; only pre-existing warnings appeared in unrelated engine files

Next: align DXA indexing with the extraction entry budget and add admission size/ratio limits for M-02.

### M-02 DXA indexing and admission budgets - complete

- Centralized the native DXA admission constants in `dxa_admission_limits.h`, with the launcher contract explicitly tied to `ExtractionLimits`
- Reduced launcher and native traversal ceilings from 65,536 archive entries and 49,152 indexed textures to the standard 4,096-entry extraction budget; directory count follows the same ceiling
- Reduced retained/indexed path storage from as much as 16 MiB to 1 MiB while retaining the 4,096-byte per-path and 64-component limits
- Launcher admission now rejects DXA sources over 2 GiB, individual expanded entries over 512 MiB, aggregate declared expansion over 2 GiB, expansion ratios over 1000:1, and non-directory entries with missing size metadata before enabling or publishing an import
- Native texture indexing independently rejects indexed files over 512 MiB or an indexed expanded total over 2 GiB, covering manually altered launcher state as well as normal admitted imports
- Generated regression inputs cover cardinality, depth, aggregate paths, source/per-entry/total byte limits, missing metadata, the exact ratio boundary, and a real high-ratio ZIP without adding a retained pathological archive
- Scoped code quality and the replacement-texture policy guard passed; the focused 7-case scanner suite passed, and the complete 1,184-case Android JVM suite passed with one existing skip
- The Android native debug build passed for D1 and D2 on arm64-v8a, armeabi-v7a, and x86_64

Next: stream and cap mod-detail archive summaries for M-03.

### M-03 streamed mod-detail summaries - complete

- Removed the eager `ZipEntry` list from DXA details; archive admission now runs first and stops after the shared 4,096-entry ceiling
- A bounded streaming pass accumulates only total file count, fixed category buckets, total category sizes, and three examples per category
- Document discovery retains at most 64 prioritized candidates while preserving the normal README/package preference; the model records the complete candidate count and the details button reports the retained/total count when truncated
- Patch detail inspection retains and parses at most the existing 64-document per-mod limit, with an explicit truncation problem if additional patch entries exist
- Generated tests cover 80-document retention/truncation with complete streamed category totals and fail-closed rejection of a 4,097-entry archive; no pathological archive was retained in the repository
- Scoped Kotlin quality, the focused 20-case mod-details/document suites, and the complete 1,186-case Android JVM suite passed with one existing skip

All findings in `security_scan_bad_level_mod_data_20261015.md` are fixed and have focused regression coverage. The retained malformed corpus remains centralized under `android/tests/fixtures/bad_test_inputs/` with its README; larger cardinality and compression cases are generated at test runtime.
