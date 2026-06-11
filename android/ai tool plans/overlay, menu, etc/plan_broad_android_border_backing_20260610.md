## Goal
Ensure the Android backing clear applies to all stale-border cases identified in the modal/window survey without adding per-call-site clears.

## Plan
- [x] Re-check the surveyed risky paths against the central draw hook.
- [x] Confirm no additional predicate change is needed.
- [x] Confirm existing scoped validation still applies because no further code changes were needed.

## Result
- Menu and join-failure dialogs are covered by `Screen_mode != SCREEN_GAME`.
- Kick/remove/consistency/save-state dialogs that suspend drawing are covered by hidden `Game_wind`.
- Death, demo, and endlevel letterbox views are covered by `CM_LETTERBOX`.
- No additional per-call-site clears were needed.
