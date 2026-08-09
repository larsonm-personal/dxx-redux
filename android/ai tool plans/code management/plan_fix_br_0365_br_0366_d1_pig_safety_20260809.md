# Fix BR-0365 and BR-0366 D1 PIG safety

## Plan

- [x] Map D1-in-D2 PIG parsing, robot publication, bitmap replacement ownership, and existing tests
- [x] Add checked D1 PIG span and cross-reference validation before live asset publication
- [x] Replace fixed unsafe frame-replacement writes with capacity-aware owned storage
- [x] Add focused malformed-PIG and aggregate-capacity regression coverage
- [x] Run scoped formatting, focused tests, Android builds, and paired host validation
- [x] Move completed findings to the done ledger with resolution evidence
- [x] Mark this plan complete

BR-0365 and BR-0366 are complete. D1-in-D2 now validates and stages the property, model, robot, weapon, effect, vclip, bitmap-reference, and sound generations before publication, reports the failing asset section at launch, and preserves sparse retail vclip and logical alternate-sound-map semantics.
