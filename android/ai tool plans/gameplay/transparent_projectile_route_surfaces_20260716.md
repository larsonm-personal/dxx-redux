# Transparent projectile route surfaces

## Plan

- [x] Reproduce Counterstrike level 2 trigger 21 rejection and identify each blocking surface.
- [x] Define general projectile traversal rules for transparent surfaces and shoot-open doors.
- [x] Implement the shared rule with focused diagnostics and native coverage.
- [x] Verify Counterstrike level 2 produces a calculated trigger 21 firing waypoint and test it on device.
- [x] Compare the full route corpus, run D1/D2 and Android builds, and finish code quality.

## Findings

- `FQ_TRANSWALL` already implements the requested approximation for a surface the
  engine classifies as transparent: the route ray crosses the whole surface and
  does not sample the animated texture pose or individual transparent pixels.
- The missing case was wall 159, an unlocked, keyless `WCF_HIDDEN` door between
  the firing area and trigger 21. Normal weapon collision opens that door, but
  the route ray treated its initial closed state as permanent.
- Added a caller-defined passable-wall hook to D1 and D2 FVI. Route analysis uses
  it only for unlocked, keyless hidden doors and never for the target wall. This
  lets a firing pose represent shooting the door open and then shooting through
  it without making the door navigable to Guidebot movement.
- A broader trial that allowed ordinary shoot-open doors changed objective order
  in Counterstrike levels 11, 13, and 14. It was discarded. The final hidden-door
  rule changes only Counterstrike level 2 in the base D2 corpus comparison.
- Counterstrike level 2 now emits trigger 21 as `shoot_switch` at firing segment
  52, followed by the red key. The checked-in metadata and host integration test
  were updated.
- Verification passed: scoped code quality, D1/D2 Windows builds,
  `test_level_metadata_scan` and `test_route_analysis_cache` for both games, the
  focused host Counterstrike test, Android `assembleDebug` for all ABIs, and the
  emulator Guidebot test through trigger 21 completion.
- The first emulator reinstall reused an old route record because the local APK
  retained build number 18130. Deleting that one stale dev record forced fresh
  analysis; production cache records are already namespaced by build number.
