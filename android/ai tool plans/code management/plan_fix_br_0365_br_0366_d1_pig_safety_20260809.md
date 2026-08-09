# Fix BR-0365 and BR-0366 D1 PIG safety

## Plan

- [x] Map D1-in-D2 PIG parsing, robot publication, bitmap replacement ownership, and existing tests
- [x] Add checked D1 PIG span and cross-reference validation before live asset publication
- [x] Replace fixed unsafe frame-replacement writes with capacity-aware owned storage
- [x] Add focused malformed-PIG and aggregate-capacity regression coverage
- [x] Run scoped formatting, focused tests, Android builds, and paired host validation
- [x] Move completed findings to the done ledger with resolution evidence
- [ ] Mark this plan complete

BR-0366 is complete. BR-0365 remains open: robot/model structural preflight is implemented and validated on the same PIG handle before live mutation, but sound/effect/vclip validation, typed launch failure propagation, and fully transactional publication still remain.
