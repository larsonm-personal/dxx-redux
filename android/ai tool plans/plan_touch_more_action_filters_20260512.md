# Touch More Action Filters - 2026-05-12

## Goal
Adjust the Android touch overlay More button so it excludes actions that should not be offered by the overflow menu for the current layout or game state.

## Plan
1. [x] Create this plan file
2. [x] Inspect current More action filtering and tests after intervening edits
3. [x] Remove save, load, game menu, and pause from More candidates
4. [x] Hide multiplayer HUD and drop flag outside multiplayer games
5. [x] Hide cycle primary or secondary when the matching weapon wheel exists
6. [x] Update focused tests and run validation

## Notes
- Keep this scoped to Android touch overlay Kotlin unless a missing native action is discovered.
- Preserve unrelated current worktree edits.
- Added `isMultiplayerGameProvider` to the overlay and wired it from `MainActivity` so multiplayer-only More actions are hidden in single player.
- Removed quick save, quick load, game menu, and pause from the More candidate list.
- Primary and secondary cycle bindings now follow the same wheel-menu presence checks as the direct weapon bindings.
- Validation: scoped ktlint passed for `TouchOverlayView.kt` and `MainActivity.kt`. The existing ktlint helper only scans `app/src/main/java`, so test formatting was covered by compile/test and diff checks.
- Validation: `:app:testDebugUnitTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest` passed.
- Validation: `git diff --check` passed for the follow-up files.