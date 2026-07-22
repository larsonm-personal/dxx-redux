# Adversarial Branch Review Done Ledger

This is the canonical archive for findings with final dispositions. The active
queue, review completion notes, open findings, and campaign closure checklist
remain in `branch_adversarial_review_ledger.md`

Finding IDs remain unique across both ledgers. Queue rows and chunk completion
notes in the active ledger may refer to findings archived here

## SIT remediation scope note

BR-0017 is the completed SIT5 fix in this tranche. Other SIT and StuffIt-related
findings, including BR-0031 through BR-0033 and SIT portions of broader findings,
remain open in the active ledger. A portion of the requested SIT work was held
back by GPT restrictions, so no unimplemented scope was marked fixed or moved
here

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

## Disposition log

- 2026-07-21: BR-0017 fixed by the Codex remediation call at the maintainer's request. Worktree evidence is the shared fixed-header bounds predicate and guarded malformed-offset test. Validation passed scoped code quality and all 8 tests in `android/tests/test_cue_iso.ps1`. Independent P1 verification remains a campaign closure action

