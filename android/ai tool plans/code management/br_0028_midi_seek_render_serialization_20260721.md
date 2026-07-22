# BR-0028 MIDI seek and render serialization plan

Date: 2026-07-21

## Goal

Resolve BR-0028 by ensuring MIDI seeking cannot mutate synthesizer or decoder
state concurrently with audio rendering, while preserving existing handmade
comments verbatim and retaining cross-platform audio behavior.

## Steps

- [x] Review the complete finding, seek/render/teardown call graph, existing audio locks, and test infrastructure.
- [x] Define one lock ownership rule covering every mutable MIDI playback-state access.
- [x] Implement seek/render serialization without recursive locking or callback-thread allocation.
- [x] Add a deterministic concurrency regression that overlaps seek and rendering and checks forward progress.
- [x] Run scoped code quality, focused concurrency tests, and the relevant full build and test suites.
- [ ] Obtain the independent P1 verification required by the review process.
- [x] Audit the implementation diff for removed handmade comments.
- [ ] Move BR-0028 to the done ledger after independent verification passes.

## Validation record

Seven focused synchronization tests pass, including ordered source-contract
checks and a deterministic render-versus-seek exclusion and forward-progress
probe. Scoped code quality passes. `:app:externalNativeBuildDebug` builds
armeabi-v7a, arm64-v8a, and x86_64, and `android/tests/test_cue_iso.ps1`
passes all 10 native suites. No new MIDI compiler warning was emitted; the
Android build retains only the pre-existing unused `be24` warning. The deleted
comment audit found no removed existing comment lines. ThreadSanitizer and a
maintained on-device rapid-seek stress run were unavailable. Independent P1
verification remains required before the finding can move to the done ledger.
