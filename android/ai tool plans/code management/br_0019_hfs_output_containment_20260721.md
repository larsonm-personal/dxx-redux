# BR-0019 HFS Output Containment

## Scope

Remediate BR-0019 across the native HFS reader and the legacy Python/machfs
reference extractor without absorbing the separate extraction-budget,
transactional-publication, dependency-integrity, cache, temporary-ownership,
or portability findings

## Plan

- [x] Reconfirm the live native and legacy path construction and existing test coverage
- [x] Reject unsafe HFS catalog components before native paths are constructed
- [x] Reject unsafe machfs names before legacy output directories or files are created
- [x] Add focused native and Python-path regression coverage
- [x] Run scoped code quality, the focused tests, and the native extraction suite
- [x] Finalize BR-0019 and move its finding and disposition to the done ledger

## Verification note

The finding was independently reproduced across R1-CHUNK-0010,
R1-CHUNK-0021, R1-CHUNK-0028, and the later R1-CHUNK-0069 legacy-script
review. Live-code inspection on 2026-07-21 confirmed both sinks remain present
before remediation
