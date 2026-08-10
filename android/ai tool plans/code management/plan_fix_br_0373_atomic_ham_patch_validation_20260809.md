# Fix BR-0373 atomic HAM patch validation

## Plan

- [x] Re-read project instructions, select the next unresolved shortlist item, and preserve existing worktree changes
- [x] Trace HAM patch parsing, mutation order, cleanup, and D1/D2 consumers
- [x] Make the complete mutation pass transactional with a pre-apply snapshot
- [x] Add malformed, truncation, and later-entry failure regression coverage
- [x] Run focused tests, scoped quality, paired Windows builds, and Android all-ABI builds
- [x] Move BR-0373 to the done ledger with validation evidence
- [x] Mark this plan complete

## Current status

Complete. The D2-only HAM patch loader snapshots every mutable table, count, bitmap mapping, and virtual-bitmap high-water value immediately before the mutation pass. Any missing value, malformed path, type or range error, unsupported field, later-operation failure, or allocation exception restores the complete base generation before failure is reported. A structural regression test inventories all captured state and enforces snapshot-before-mutation and restore-before-report ordering. The focused transaction tests, scoped quality wrapper, Python compilation, paired Windows builds, and Android debug builds for arm64-v8a, armeabi-v7a, and x86_64 passed. The existing Windows build trees had no registered CTest tests. BR-0373 is recorded in the done ledger.
