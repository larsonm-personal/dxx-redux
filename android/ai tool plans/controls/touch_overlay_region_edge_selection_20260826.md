# Touch overlay region edge selection

## Goal

Allow taps in the launcher touch overlay editor to cycle through region edges, and allow a selected edge to be dragged.

## Plan

- [x] Locate the editor hit testing, tap selection cycle, region edge model, and drag update path
- [x] Add region edges to tap selection while preserving the existing cycle order and overlap behavior
- [x] Route drag gestures for selected edges through the existing constrained region update logic
- [x] Add or extend focused tests for edge selection cycling and dragging
- [x] Run scoped formatting, focused tests, and the required build/test verification

## Notes

- Preserve unrelated work in the current worktree
- Keep the change inside the Android launcher unless inspection shows a shared control model change is required
- Region edge hits now follow discrete controls in the tap cycle, while broad region bodies retain their existing fallback priority
- Verification passed: scoped code quality, `TouchEditorZoneEdgeTest`, and `:app:assembleDebug` including all configured native ABIs

## Follow-up: Larger edge drag target

- [x] Replace the small control-derived edge hit slop with an approximately 3% screen-width target
- [x] Verify vertical and horizontal edge hit geometry just inside and outside the expanded target
- [x] Run scoped formatting, focused tests, and Android build verification

The follow-up uses a 3% canvas-width hit slop on both sides of every edge, with a 12 px minimum. `TouchEditorZoneEdgeTest` and `:app:assembleDebug` passed after the shared Gradle build tree became idle.
