# BR-0050 HFS allocated leaf traversal

## Goal

Enumerate only structurally reachable HFS catalog leaf nodes and reject stale,
free, cyclic, out-of-range, or inconsistently linked nodes before decoding
catalog records.

## Plan

- [x] Read repository instructions, both review ledgers, the complete finding,
      related HFS findings, and the live parser and tests
- [x] Confirm the classic HFS B-tree header and node-link layout against an
      authoritative format reference
- [x] Parse and validate the catalog header node, node size, total-node count,
      first and last leaf metadata, and leaf record count
- [x] Traverse the validated forward leaf chain with allocation, range, cycle,
      kind, height, and backward-link checks
- [x] Add synthetic stale, duplicate, free, cyclic, out-of-range, and valid
      multi-leaf regression cases
- [x] Run scoped code quality, focused HFS tests, the complete native extraction
      suite, and Android ABI builds
- [x] Finalize BR-0050 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Apple TN1150 B-tree header, node-link, allocation map, and classic HFS node
  size reference: REVIEWED
- Scoped code quality: PASS
- Focused HFS suite: PASS, 10/10 including both known Mac discs
- `android/tests/test_cue_iso.ps1`: PASS, 13/13 native extraction suites
- `:app:externalNativeBuildDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64
