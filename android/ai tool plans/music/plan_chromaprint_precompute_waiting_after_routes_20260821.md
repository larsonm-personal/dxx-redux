# Chromaprint precompute waiting after route completion

## Goal

Fix mission-audio Chromaprint work that remains permanently queued after route
metadata precompute reaches completion, including after leaving and reopening
Advanced settings.

## Work plan

- [x] Confirm the stalled scheduler state and source-identity mismatch
- [x] Make route and music jobs share one stable mission-owner identity
- [x] Add regression coverage for durably extracted mission music eligibility
- [x] Run scoped code quality, focused tests, and the Android debug build

## Status

The coordinator now preserves the owning mod archive as the scheduler identity
for routes read from durable extraction storage. Mission music already uses that
archive identity, so it becomes eligible as soon as all of its routes are
terminal instead of waiting forever on a nonexistent identity match.

Scoped Kotlin formatting, the route scheduling/ordering/monitor unit tests, and
the full debug APK build all pass.
