# Fix BR-0376 custom RLE bitmap validation

## Plan

- [x] Re-read project instructions, confirm BR-0376 remains open, and preserve existing worktree changes
- [x] Trace D1 custom bitmap parsing, RLE layout consumers, replacement ownership, and existing tests
- [x] Validate complete bounded RLE row structure before allocating or publishing a replacement
- [x] Add focused valid and malformed raw/RLE custom bitmap regression coverage
- [x] Run scoped quality, focused tests, complete Windows builds, and Android all-ABI builds
- [x] Move BR-0376 to the done ledger with validation evidence
- [x] Mark this plan complete

## Current status

Complete. A reusable bounded RLE validator checks the declared size, complete row table, row spans, run operands, final terminators, and exact decoded width. D1 custom loading rejects unsupported large-row encodings, stages every resolved bitmap and validates all entries before the first replacement commit, and uses checked-width offsets and raw sizes. The focused C and structural tests, complete Windows builds, Android all-ABI builds, and the maintained Trine 2 D1-in-D2 custom-texture automation passed.
