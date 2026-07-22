# BR-0178 STi2 Method 14 Code Lengths

## Goal

Reject malformed StuffIt method 14 prefix-code lengths before any undefined
shift or invalid tree construction can occur

## Plan

- [x] Separate decoded-length validation and tree construction from bit input
- [x] Reject empty, over-width, oversubscribed, colliding, and invalid-branch
  code sets before symbol decoding
- [x] Add test-only boundary coverage for lengths 0, 31, 32, 33, and 37 and
  malformed canonical sets
- [x] Run scoped code quality and the complete native extraction suite
- [x] Finalize BR-0178 and move it to the done ledger with validation evidence
