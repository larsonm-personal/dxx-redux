# SIT5 Entry Header Bounds Fix

## Scope

Resolve BR-0017 by preventing SIT5 entry fields from being read until the full
48-byte fixed header is known to fit within the archive buffer.

## Plan

- [x] Add an overflow-safe fixed-header bounds check shared by SIT5 entry parsing
  and parent-offset lookup
- [x] Add regression coverage for truncated entry offsets at the end of an archive
- [x] Run focused formatting and native extraction tests
- [x] Mark BR-0017 fixed and move it to the adversarial review done ledger with
  validation evidence

## Result

`sit5_entry_header_fits` now validates the complete fixed header before any
entry field is read. The malformed regression places the archive against a
guard page and checks offsets 52 through 99. Scoped code quality and all 8
native extraction suites passed
