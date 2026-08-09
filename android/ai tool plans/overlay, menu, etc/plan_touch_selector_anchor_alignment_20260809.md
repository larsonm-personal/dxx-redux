# Touch selector anchor alignment

## Goal

Align the music track-selector selection area over its previous/next buttons
in the touch editor and use the same anchor in the in-game overlay.

## Plan

- [x] Trace the editor and runtime selector anchors and identify the offset
- [x] Centralize the anchor calculation and apply it to editor and runtime
- [x] Add or update focused geometry tests for the shared anchor behavior
- [x] Run scoped formatting, unit tests, Android native build, and emulator
  installation/smoke verification
- [x] Mark the work complete and record any remaining visual tuning

## Result

The saved music control position is now the center of its previous/next button
pair in both the editor and the in-game overlay. The editor selection rectangle
and hit area use the same shared geometry. No remaining alignment tuning is
known.
