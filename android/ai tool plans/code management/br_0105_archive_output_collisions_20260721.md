# BR-0105 archive output collision plan

Date: 2026-07-21

## Goal

Resolve BR-0105 by rejecting archive entries whose normalized output paths
collide before extraction can create or replace any destination, while
preserving existing handmade comments verbatim.

## Steps

- [x] Review the full finding, affected extraction paths, normalization rules, and existing tests.
- [x] Define one collision policy appropriate to the affected archive output namespaces.
- [x] Preflight all selected destinations and reject duplicates before filesystem mutation.
- [x] Add high-level fixtures for exact, case-folded, separator-normalized, and flattened collisions as applicable.
- [x] Run scoped code quality, focused collision tests, and the complete relevant test suites.
- [x] Audit the diff for removed comments and move BR-0105 to the done ledger only if validation passes.

## Validation record

Scoped Kotlin code quality passed. Thirty focused mission and archive tests passed
with one environment-gated RAR fixture skipped, and all 10 native extraction
suites passed. A broader 43-test mission run passed 42 tests; the remaining
large-HOG test is rejected before this code by the existing BR-0018 1000:1
expansion-ratio ceiling. The deleted-comment audit found no removed existing
comment lines.
