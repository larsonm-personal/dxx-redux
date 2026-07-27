# BR-0051 HFS partition and extent bounds

## Goal

Keep every HFS metadata and fork read inside the selected partition and the
Master Directory Block's declared allocation-block range using checked
partition-relative arithmetic.

## Plan

- [x] Read repository instructions, both review ledgers, the complete finding,
      related HFS findings, and the live parser and tests
- [x] Trace track, partition, MDB, allocation-area, catalog, and file-fork
      offsets and define one checked interval contract
- [x] Store and validate the immutable partition interval and MDB allocation
      block count during volume loading
- [x] Reject every catalog or file extent outside the allocation-block range or
      partition before any read or output creation
- [x] Add exact-boundary, one-byte, one-block, overflow, MDB, allocation-area,
      catalog, and data-extent regression cases
- [x] Run scoped code quality, focused HFS tests, the complete native extraction
      suite, and Android ABI builds
- [x] Finalize BR-0051 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality: PASS
- Focused HFS suite: PASS, all 7 runnable tests; 4 known-media cases skipped
  because the sample discs were absent
- `android/tests/test_cue_iso.ps1`: PASS, all 13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
