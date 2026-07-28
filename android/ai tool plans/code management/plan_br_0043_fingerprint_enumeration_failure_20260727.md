# BR-0043 fingerprint enumeration failure

## Goal

Make audio fingerprinting fail closed when directory enumeration is incomplete,
so partial scans cannot produce apparently valid database updates.

## Plan

- [x] Create the required implementation plan
- [ ] Read the complete finding and trace enumeration and publication paths
- [ ] Implement checked, fail-closed directory traversal
- [ ] Add focused incomplete-enumeration and valid-scan coverage
- [ ] Run scoped quality, focused tests, native suites, and platform builds
- [ ] Record the resolution and move BR-0043 to the done ledger

## Verification

- Pending
