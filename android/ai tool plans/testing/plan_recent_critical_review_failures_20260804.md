# Recent critical review test failures

## Goal

Investigate the failures from the supplied full test run, prioritizing recent
SAF picker, CD parsing, extraction, file handling, and limit changes while
distinguishing deterministic regressions from emulator infrastructure faults.

## Plan

- [x] Create the investigation plan
- [x] Read repository instructions and the complete supplied report
- [x] Inspect each failure log and its relevant predecessor state
- [x] Trace likely failures to recent critical-review changes
- [x] Implement narrow fixes or record justified dispositions
- [x] Validate deterministic fixes and representative device ordering
- [x] Run scoped quality checks and record results

## Findings

- Two Release native tests compiled setup expressions out through `NDEBUG`.
  Test targets now explicitly undefine it.
- Ordered setup broadcasts did not wait for asynchronous command and
  introspection work. Commands now keep the ordered result open, and
  introspection files use atomic publication.
- Setup button discovery scanned only Compose semantics IDs and omitted button
  text child nodes. The bounded accessibility scan now includes those nodes.
- The guidebot boss-teleport invalidation cleared the route without replanning.
  It now refreshes metadata and selects the next goal immediately.
- One metadata zero had an empty display string, and the route corpus baseline
  was stale after reviewed route changes. Both checked-in results were updated.
- The controls readability test contained an impossible width threshold and an
  unstable absolute scroll coordinate. It now checks feasible size and semantic
  scroll offset.
- GOG wrapper tests left large installers in `/data/local/tmp`, which caused a
  later Mac BIN copy to truncate. Both wrappers now clean up their own staged
  installers, and the Mac test verifies both copies byte-for-byte by size.
- SAF disc storage estimation opened and discarded a pipe-backed URI before
  extraction. Preparation now retains and consumes the same descriptor.
- Mission ZIP music cache was age-bounded but not size-bounded and reached
  548 MiB during the batch. It now evicts oldest generations under a 256 MiB
  limit, while the Mac test clears this specific disposable predecessor cache.

## Validation

- Full Android unit-test task and debug APK assembly passed with JDK 21.
- Release `graphics_config_transaction_tests` and
  `chromaprint_db_config_tests` passed after rebuilding their targets.
- Mac SAF extraction passed twice consecutively, each run covering seekable
  then pipe-backed URIs and staging all 718,912,320 bytes.
- Full guidebot route-next, launcher DPAD, debug-log refresh, controls
  readability, music controls, both GOG wrappers, process-wait helper, travel
  time corpus, and route corpus tests passed in focused validation.
- Both GOG wrappers passed and left no staged installer. The Mac test then
  passed twice with prior batch state still present after cleaning only the
  predecessor cache it explicitly owns for this scenario.
- Scoped mixed-language formatting and lint checks passed. `git diff --check`
  passed.
