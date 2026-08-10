# Fix BR-0370 classic-demo source and output aliasing

## Plan

- [x] Re-read project instructions, confirm BR-0370 remains open, and preserve existing worktree changes
- [ ] Trace classic-demo command parsing, PhysFS input resolution, JSON output publication, and existing tests
- [ ] Reject source/output identity before destructive operations and publish through an owned temporary file
- [ ] Add focused exact-path, canonical-alias, hard-link, malformed-input, and successful-conversion coverage
- [ ] Run scoped quality, focused tests, paired Windows builds, and Android all-ABI builds
- [ ] Move BR-0370 to the done ledger with validation evidence
- [ ] Mark this plan complete

## Current status

BR-0370 is open. Investigation is starting from the D2 classic-demo command adapter and `newdemo_dump_json` implementation named in the ledger.
