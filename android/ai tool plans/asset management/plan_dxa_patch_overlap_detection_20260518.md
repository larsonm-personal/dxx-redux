# DXA patch overlap detection plan

## Goal
- Allow multiple enabled HAM patch DXAs when their RFC 6902 operations touch disjoint semantic fields
- Keep launch blocked for real overlapping edits, missing patch files, unreadable patches, or unsupported patch shapes
- Add an order-independence test for applying UUD2SP and UUD2TP patches to the same retail D2 HAM baseline

## Tasks
- [x] Read current mod conflict detection and UUD patch generation/test helpers
- [x] Add patch-level overlap detection for same-file patch paths
- [x] Add or extend tests for non-overlap, overlap, and UUD2SP/UUD2TP ordered application
- [x] Run focused validation and format touched files