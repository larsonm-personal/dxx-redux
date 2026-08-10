# Fix BR-0370 classic-demo source and output aliasing

## Plan

- [x] Re-read project instructions, confirm BR-0370 remains open, and preserve existing worktree changes
- [x] Trace classic-demo command parsing, PhysFS input resolution, JSON output publication, and existing tests
- [x] Reject source/output identity before destructive operations and publish through an owned temporary file
- [x] Add focused exact-path, canonical-alias, hard-link, malformed-input, and successful-conversion coverage
- [x] Run scoped quality, focused tests, paired Windows builds, and Android all-ABI builds
- [x] Move BR-0370 to the done ledger with validation evidence
- [x] Mark this plan complete

## Current status

Complete. The converter opens a stable source handle first, compares existing destinations by platform file identity, decodes before creating output, writes to an exclusively owned sibling temporary file, syncs and closes it, rechecks destination identity, and atomically replaces only a distinct destination. The reusable integration test passed exact, relative/absolute, and hard-link alias rejection; malformed-input output preservation; successful 5,165-record conversion; and temporary cleanup. Paired Windows and Android all-ABI builds passed.
