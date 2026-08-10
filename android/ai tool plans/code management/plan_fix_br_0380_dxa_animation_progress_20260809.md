# Fix BR-0380 DXA animation progress validation

## Plan

- [x] Re-read project instructions, select BR-0380 as the next native format issue, and preserve existing worktree changes
- [x] Trace every patched vclip, effect, and wall-clip field into its runtime consumers
- [x] Validate complete staged animation records before each mutation commits
- [x] Add full-row and field-operation boundary regression coverage
- [x] Run focused tests, scoped quality, paired Windows builds, and Android all-ABI builds
- [x] Move BR-0380 to the done ledger with validation evidence
- [x] Mark this plan complete

## Current status

BR-0380 is fixed and archived. Five focused tests, Python compilation, scoped quality, paired Windows builds, Android debug builds for all configured ABIs, and `git diff --check` passed.
