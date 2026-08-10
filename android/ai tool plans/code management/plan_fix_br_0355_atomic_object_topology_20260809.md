# Fix BR-0355 atomic object topology restoration

## Plan

- [x] Re-read project instructions, confirm BR-0355 remains open, and preserve existing worktree changes
- [x] Trace paired D1 and D2 object-link restore, validation, and compatibility fallback paths
- [x] Make malformed topology validation non-mutating and rebuild any fallback from an explicitly empty graph
- [x] Add focused valid and malformed topology regression coverage for both engines
- [x] Run scoped quality, focused tests, paired Windows builds, and Android all-ABI builds
- [x] Move BR-0355 to the done ledger with validation evidence
- [x] Mark this plan complete

## Current status

Complete. Paired topology validation now uses local candidate segment heads and publishes them only after validating roots, reciprocal links, cycles, segment ownership, and complete live-object coverage. Any compatibility relink starts by clearing all segment heads. Fourteen focused save-validation tests, complete paired Windows builds, Android all-ABI builds, and the maintained 2,430-frame D1-in-D2 checkpoint replay passed.
