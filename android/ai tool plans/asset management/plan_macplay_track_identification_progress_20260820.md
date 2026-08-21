# MacPlay track identification progress

## Goal

Make the MacPlay disc-import progress indicator advance at least once per
audio track while tracks are being identified.

## Work plan

- [x] Trace the MacPlay identification loop and existing import progress model.
- [x] Report deterministic per-track identification progress through the
  existing callback without changing identification behavior.
- [x] Add focused regression coverage for progress granularity and completion.
- [x] Run scoped code quality, focused tests, and an Android debug build.

## Status

Complete. The focused JVM test, scoped code quality, and the three-ABI Android
debug build passed.
