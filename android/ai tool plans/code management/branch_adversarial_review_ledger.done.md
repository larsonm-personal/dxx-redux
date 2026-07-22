# Adversarial Branch Review Done Ledger

This is the canonical archive for findings with final dispositions. The active
queue, review completion notes, open findings, and campaign closure checklist
remain in `branch_adversarial_review_ledger.md`

Finding IDs remain unique across both ledgers. Queue rows and chunk completion
notes in the active ledger may refer to findings archived here

## SIT remediation scope note

BR-0017, BR-0031, and BR-0178 are the completed SIT and STi2 fixes in this
tranche. Other SIT and StuffIt-related findings, including BR-0032, BR-0033,
and the incomplete BR-0018 scope, remain open in the active ledger. A portion
of the requested work was held back by GPT restrictions, so no unimplemented
scope was marked fixed or moved here

BR-0017 is code-complete and guard-page tested. The independent verification
call required for every P1 by the campaign process has not yet been recorded,
so campaign closure must still obtain that independent verification

## Finalized findings

### BR-0017: P1 - Validate a complete SIT5 entry header before reading its parent offset

- [x] FIXED
- Type: defect
- Confidence: high
- Category: security/memory-safety
- Found by: R1-CHUNK-0009
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/stuffit_extract.c:L331-L334` in `sit5_parse_entries`
- Related: `android/app/src/main/cpp/extract/stuffit_extract.c:L178-L190,L354-L369`, `android/app/src/main/cpp/extract/jni_disc_import.c:L481-L502`, and `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt:L378-L464`
- Evidence: The loop rejects only `current_offset >= archive_size`, then immediately reads four bytes from `archive_data + current_offset + 26`. The complete-entry check in `sit5_parse_entry` runs only after that read. The archive header controls `first_offset`, and `sit5_list_entries` accepts every value below `archive_size`; therefore an offset in the final 29 bytes passes the first check but makes the parent-offset read cross the heap buffer boundary
- Trigger: Import a 100-byte SIT5-looking file with a nonzero root count and `first_offset` set near the end of the file, such as 99
- Impact: A user-selected malformed archive causes an out-of-bounds native heap read before parsing can reject it, which can crash the launcher process and may expose adjacent-memory behavior to further parser exploitation
- Expected: No entry field is read until the parser has established that the entire minimum fixed header is present, using overflow-safe bounds arithmetic
- Suggested fix: Before reading `parent_offset` or calling the entry parser, require `current_offset <= archive_size - 48` after first proving `archive_size >= 48`. Centralize fixed-header validation so all field reads share the same checked span, and use subtractive checks for subsequent variable-length sections
- Validation: Add malformed fixtures for every offset from `archive_size - 1` through the first valid 48-byte boundary, assert clean rejection, and run the parser tests under AddressSanitizer or an equivalent native bounds checker
- Resolution: Fixed in the 2026-07-21 worktree by centralizing the subtractive 48-byte span check in `sit5_entry_header_fits` and applying it before both entry parsing and parent-offset access. `test_stuffit_malformed` places an exact 100-byte archive at a guard-page boundary and rejects every `first_offset` from 52 through 99. Scoped code quality passed, and `android/tests/test_cue_iso.ps1` passed all 8 native suites, including malformed SIT5, STi2, StuffIt corpus, and demo-oracle coverage

### BR-0019: P1 - Reject HFS dot components before constructing output paths

- [x] FIXED
- Type: defect
- Confidence: high
- Category: security/path-traversal
- Found by: R1-CHUNK-0010, R1-CHUNK-0028, R1-CHUNK-0069
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/mac_hfs_extract.c:L152-L169` in `extract_hfs_matching_files`
- Related: `android/app/src/main/cpp/extract/mac_hfs_extract.h:L13-L18`, `android/app/src/main/cpp/extract/hfs_reader.c:L183-L200,L345-L377,L624-L663,L669-L704`, `android/app/src/main/java/com/dxxredux/app/SetupDiscImport.kt:L237-L285`, and `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt:L1261-L1294`
- Evidence: HFS catalog names are untrusted bytes. `copy_catalog_name` replaces slash and backslash but preserves names equal to `.` or `..`; `build_catalog_path` then joins those components with `/`. The assigned extractor accepts a file when its basename has a game extension and concatenates the complete catalog path directly after `output_dir`. A directory named `..` containing `escape.hog` therefore produces `<output_dir>/../escape.hog`, which `mkdirs_for_file` and `open` resolve outside the selected set directory
- Trigger: Import a crafted HFS data track whose catalog contains a directory named `..` and a child file such as `escape.hog` with valid extents
- Impact: The archive can create or truncate extension-matching files in parent directories outside the chosen file set, corrupt another set or other app-private game data, and evade the later hoisting and cleanup that walk only within the selected set
- Expected: Every extracted destination is derived from normalized safe components and is proven to remain beneath the intended output root before any directory creation or file open
- Suggested fix: Reject empty, `.`, and `..` catalog components during decoding, build destinations through one checked path helper, and verify the normalized destination has the canonical output directory as its parent prefix. Prefer flattening approved game basenames when directory structure is not required
- Validation: Add a synthetic HFS fixture with `.`, `..`, separator, and ordinary nested components; assert malicious entries are rejected without creating anything outside a temporary output root, and run the test on Windows and a POSIX host to cover both path implementations
- Additional location (R1-CHUNK-0069): `game_data/extract_mac_cd.ps1:L296-L326` in the inline machfs extraction program
- Additional evidence (R1-CHUNK-0069): machfs decodes HFS catalog names to strings and exposes them through `Volume.items()` without rejecting `.`, `..`, slash, or backslash. The inline Python recursively passes each name to `os.path.join` before `os.makedirs` or `open`, with no canonical containment check. A crafted folder named `..`, or a native-separator name, therefore escapes `_mac_extract_temp/hfs_files`; the script's `finally` removes only `_mac_extract_temp`, leaving the outside write behind.
- Additional validation (R1-CHUNK-0069): Run the inline extractor against synthetic machfs objects and serialized HFS fixtures containing dot, rooted, drive-qualified, forward-slash, backslash, mixed-separator, and ordinary components; require rejection before creation, unchanged sentinel bytes outside the unique root, and successful extraction of normal catalog names on Windows and POSIX.
- Resolution: Fixed in the 2026-07-21 worktree. Native HFS catalog decoding now rejects empty, `.`, and `..` file and directory components before entries can reach path construction, with a second path-builder guard. The legacy oracle script now calls a tracked Python helper that rejects empty, dot, rooted, drive-qualified, NUL, forward-slash, and backslash names and proves each canonical child remains strictly below the selected extraction root before creating it. Focused Python tests preserve an outside sentinel while rejecting malicious names and extract an ordinary nested file. The real-media HFS suite passes all seven tests, scoped code quality passes, PowerShell AST parsing reports zero errors, Python byte-compilation passes, and `android/tests/test_cue_iso.ps1` passes all nine native extraction suites. The independent R1-CHUNK-0021 and R1-CHUNK-0069 reviews reconfirmed the original native and legacy sinks before remediation

### BR-0031: P2 - Reject encrypted STi2 entries before extraction

- [x] FIXED
- Type: defect
- Confidence: high
- Category: correctness/compatibility
- Found by: R1-CHUNK-0014
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/sti2_extract.c:L323-L345,L1999-L2010` in `is_valid_method` and `sti2_list_entries`
- Related: `android/app/src/main/cpp/extract/sti2_extract.c:L44-L49,L1879-L1942` and `android/app/src/main/cpp/extract/sti2_extract.h:L19-L41`
- Evidence: The parser defines the StuffIt encrypted flag, accepts a method byte after examining only its low compression bits, then stores only `method & 0x0f` in the public entry. It does not retain an encrypted bit, padding, or entry key. Extraction therefore treats encrypted method `0x80` as stored method 0 and copies encrypted bytes as successful output, or sends encrypted bytes to methods 13 through 15 as if they were compressed plaintext. The referenced XAD parser instead marks these entries encrypted and handles their 16-byte key and padding metadata
- Trigger: Import a valid password-protected legacy StuffIt or STi archive containing a data fork whose filename matches a requested game extension
- Impact: The importer can report a successfully extracted game file that is ciphertext or decoder garbage, or fail with no indication that encryption rather than archive corruption is unsupported
- Expected: Unsupported encrypted entries are identified during listing and rejected before allocation or output creation; any future password support preserves and consumes all required encryption metadata
- Suggested fix: Add an explicit encrypted field to the internal and public entry metadata, preserve the full method flags, reject encrypted matching entries with a distinct unsupported-encryption result, and only add decryption after the key, padding, and password contract is implemented
- Validation: Build a valid-header fixture for stored and compressed entries with `STI2_ENCRYPTED_FLAG`, including compressed sizes below and above the 16-byte key requirement, and assert listing records encryption while extraction creates no output and returns the documented unsupported status
- Resolution: Fixed on 2026-07-21. Public entry metadata now preserves data- and resource-fork encryption flags, direct and matching extraction return `STI2_EXTRACT_UNSUPPORTED_ENCRYPTION` before allocation or filesystem mutation, and matching extraction propagates the distinct status. Valid-header fixtures cover encrypted stored data and encrypted method 13 data with compressed sizes below and above 16 bytes, verify listing metadata, and verify that neither extraction path creates output. Scoped code quality passed and all 9 native extraction tests passed

### BR-0178: P2 - Reject oversized StuffIt method 14 code lengths

- [x] FIXED
- Type: defect
- Confidence: high
- Category: correctness/undefined-behavior
- Found by: R1-CHUNK-0033
- Location: `c01d8fe4686c63d931b1e543a6305bbafaa944a9:android/app/src/main/cpp/extract/sti2_extract.c:L1022-L1130` in `method14_read_tree`, especially L1094-L1107
- Related: `android/app/src/main/cpp/extract/sti2_extract.c:L699-L733,L1133-L1225`, `android/app/src/main/cpp/extract/test_sti2.c:L313-L508`, upstream XADMaster `XADStuffItOldHandles.m:L141-L216`, and BR-0033
- Evidence: Method 14 derives each code length from attacker-controlled `j`, `o`, and symbol fields, allowing values above the 32-bit width of `buff` and the canonical-code accumulator. After sorting the lengths, L1102 shifts the 32-bit unsigned `j` by the difference between adjacent lengths without bounding that difference. The encoding can choose the zero marker with `j = 5` and `o = 8`, emit 307 zero lengths, then emit a final length of 37; canonical construction then evaluates `j <<= 37`. A shift count greater than or equal to the promoted operand width is undefined C behavior. The imported XAD routine contains the same unchecked operation, so copying it into this branch did not supply a malformed-input guard
- Trigger: Import a StuffIt entry using method 14 whose first tree encodes a long code-length gap, such as zero lengths followed by length 37, before any file payload is produced
- Impact: A user-selected malformed archive invokes undefined native behavior during tree construction. Results vary by compiler and architecture and can include sanitizer termination, inconsistent tree construction, decoder failure, or, when combined with the missing integrity enforcement in BR-0033, acceptance of platform-dependent output
- Expected: Method 14 rejects every code length or adjacent-length delta that cannot be represented safely by its 32-bit canonical-code and tree representation before performing a shift
- Suggested fix: Define the supported maximum code length from the representation width, reject decoded lengths above it, validate adjacent deltas before shifting, and reject oversubscribed, incomplete, or colliding canonical sets before symbol decoding. Keep hard compressed-input exhaustion and output integrity coordinated with BR-0033
- Validation: Add method 14 tree fixtures with lengths 0, 31, 32, 33, and 37, large adjacent gaps, oversubscribed and incomplete sets, and the zero-marker sequence above; assert prompt failure without output for every invalid tree and stable valid decoding under AddressSanitizer and UndefinedBehaviorSanitizer
- Resolution: Fixed on 2026-07-21 by moving canonical method 14 tree construction into `method14_build_tree`. It rejects empty trees, lengths above 32 bits, unsafe shifts, canonical-code overflow, oversubscribed sets, leaf collisions, out-of-range nodes, and attempts to traverse a leaf as a branch before symbol decoding. Representable incomplete trees remain supported. Test-only coverage verifies empty input rejection, valid 31- and 32-bit boundaries, rejection at 33 and 37 bits, a valid complete one-bit tree, and an oversubscribed one-bit tree. Scoped code quality passed and all 9 native extraction tests passed. Sanitizer execution was not available in this Windows toolchain

## Disposition log

- 2026-07-21: BR-0017 fixed by the Codex remediation call at the maintainer's request. Worktree evidence is the shared fixed-header bounds predicate and guarded malformed-offset test. Validation passed scoped code quality and all 8 tests in `android/tests/test_cue_iso.ps1`. Independent P1 verification remains a campaign closure action
- 2026-07-21: BR-0019 fixed by the Codex remediation call at the maintainer's request. Native catalog components fail closed before path construction, and the legacy machfs helper validates canonical containment before filesystem mutation. Focused Python and real-media HFS tests passed, along with scoped code quality and all 9 native extraction suites
- 2026-07-21: BR-0031 fixed by the Codex remediation call at the maintainer's request. Encryption metadata is preserved and encrypted matching entries return a distinct unsupported status before allocation or output creation. Scoped code quality and all 9 native extraction tests passed
- 2026-07-21: BR-0178 fixed by the Codex remediation call at the maintainer's request. Method 14 canonical-tree construction now rejects unrepresentable, oversubscribed, colliding, and invalid-branch code sets before unsafe shifts or symbol decoding. Scoped code quality and all 9 native extraction tests passed; sanitizer execution was unavailable in the Windows toolchain
