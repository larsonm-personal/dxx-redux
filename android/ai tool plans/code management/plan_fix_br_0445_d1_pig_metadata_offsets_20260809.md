# Fix BR-0445 D1 PIG metadata offset validation

## Plan

- [x] Re-read project instructions, confirm BR-0445 remains open, and preserve existing worktree changes
- [x] Trace D1 PIG metadata parsing and direct, UI, and automation callers
- [x] Centralize subtraction-safe table-span validation before every byte-array read
- [x] Add boundary fixtures for signed offsets, counts, exact fits, and one-byte truncation
- [x] Run focused tests, scoped quality, Android unit tests, and Android all-ABI builds
- [x] Move BR-0445 to the done ledger with validation evidence
- [x] Mark this plan complete

## Current status

Complete. The parser validates the count span before reading it, computes the complete header span in `Long`, and narrows offsets only after proving they fit the materialized byte array. Both declared-offset and offset-zero layouts use this shared validation. Boundary tests cover short input, negative and near-maximum offsets, both valid count-span boundaries, an exact header fit, and a one-byte truncation. The focused metadata tests, complete debug unit suite, scoped quality checks, and Android debug assembly for arm64-v8a, armeabi-v7a, and x86_64 passed. BR-0445 is recorded in the done ledger.
