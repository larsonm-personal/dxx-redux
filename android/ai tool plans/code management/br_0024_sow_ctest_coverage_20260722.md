# BR-0024 SOW CTest coverage

## Goal

Make the standard native test suite fail on SOW extraction count, content,
append-order, scan, or malformed-input regressions instead of merely building
an unchecked smoke executable

## Plan

- [x] Read repository instructions and the complete BR-0024 finding
- [x] Inventory existing synthetic SOW coverage, real-media fixtures, output
  oracles, split-archive behavior, and the post-ISO scan boundary
- [x] Add assertion-based real and split SOW extraction coverage with explicit
  fixture skip behavior and register it with CTest
- [x] Add focused scan and failure-status coverage at the callable boundary
- [x] Prove the assertions fail on a deliberately wrong oracle, then restore
  the correct oracle
- [x] Run scoped code quality, the full native suite, and relevant build checks
- [x] Finalize the finding disposition and validation record

## Validation record

- `ctest -N` lists `sow_huffman_tests`, `sow_integrity_tests`, and
  `sow_real_media_tests`
- The real-media suite verifies exact retail and split counts plus seven
  SHA-256 hashes and explicitly skips when the required media is unavailable
- A deliberately incorrect retail HOG hash made the oracle exit 1
- Synthetic coverage includes stored, compressed, malformed, truncated, scan,
  invalid-argument, and cancellation behavior
- Scoped code quality and direct CMake formatting and linting passed
- All 13 registered native extraction suites passed
- BR-0072 remains the owner for scan overflow, unreadable trees, link cycles,
  and deterministic ordering
