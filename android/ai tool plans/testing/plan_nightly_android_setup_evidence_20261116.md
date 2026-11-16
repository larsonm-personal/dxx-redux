# Nightly Android setup evidence and stale contracts

## Problem

The Nov15/16 sampled Android batch exited before per-test evidence initialization. All four owners remained NOT_RUN and retail-safe artifacts retained no differentiating setup information. The precise setup failure is not known. D2 AI restore and XL mixer source tests also still assert implementation shapes replaced by functional features.

## Changes

- Write an atomic startup-stage sidecar before master initialization and each meaningful prerequisite, build, installation and launcher-readiness boundary
- Collect it only for missing test observations from an attempted master process; retain only fixed stage codes through the retail sanitizer
- Keep successful sibling evidence and all coverage/status semantics intact; do not publish raw logs, paths, exception messages or assets
- Update D2 restore tests to validate staged bounds, exact reads, rollback and context-checked publication; retain D1 checks
- Allow only the guarded functional XL mixer format negotiation while still forbidding duplicated diagnostic queries

## Verification

Run focused source contracts and nightly planner/retail tests, catalog validations, and scoped code quality. Emulator reproduction remains separately required on the exact four-owner batch with pinned target/assets; stage evidence diagnoses setup rather than claiming it is repaired. No engine behavior is changed.
