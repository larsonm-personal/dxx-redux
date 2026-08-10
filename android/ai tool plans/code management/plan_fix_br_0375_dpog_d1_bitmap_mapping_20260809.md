# Fix BR-0375 DPOG D1 bitmap mapping

## Plan

- [x] Re-read project instructions, confirm BR-0375 remains open, and preserve existing worktree changes
- [x] Trace DPOG target decoding, D1-to-D2 bitmap mapping, publication, and teardown
- [x] Make the explicit D1 replacement index authoritative and reject unmapped targets
- [x] Add target-identity, collision, range, and teardown regression coverage
- [x] Run focused tests, scoped quality, paired Windows builds, and Android all-ABI builds
- [x] Move BR-0375 to the done ledger with validation evidence
- [x] Mark this plan complete

## Current status

Complete. The DPOG loader treats its explicit D1 replacement index as authoritative and resolves it only through the D1-to-D2 wall-bitmap map populated by `load_d1_bitmap_replacements`. D2 name lookup remains only for PPIG files without a replacement table. The resolver accepts an `int`, checks the D1 index before map access, rejects missing mappings, validates the converted D2 texture number and final bitmap slot, and returns no raw-index fallback. Regression tests enforce target-source selection, range checks, load ordering, exact-slot publication, and teardown restoration structure. Six focused mapping and staging tests, Python compilation, the scoped quality wrapper, paired Windows builds, and Android debug builds for arm64-v8a, armeabi-v7a, and x86_64 passed. BR-0375 is recorded in the done ledger.
