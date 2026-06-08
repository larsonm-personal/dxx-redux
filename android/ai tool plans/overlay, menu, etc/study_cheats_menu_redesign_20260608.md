# Study: cheats menu redesign

## Goal
- Study how to remove the touch-overlay cheat action and add a cheats entry to the overlay settings menu
- Selecting the entry should pause single-player games and show a vertical scrollable cheat list with a fixed Back action at the top
- Do not implement source changes during this pass

## Status
- [x] Create study plan
- [x] Trace current touch overlay cheat action
- [x] Trace overlay settings menu implementation
- [x] Trace cheat definitions and invocation path
- [x] Identify implementation options and test coverage
- [x] Summarize recommended change list

## Current Implementation
- `TouchBindings.BTN_CHEATS_MENU = 100` is an overlay-only binding shown in button pickers as `Cheats Menu`
- `TouchOverlayView.pressLayoutButtonBinding()` toggles `cheatsOverlayOpen` when a layout button with that binding is pressed
- `TouchOverlayView.drawCheatsOverlay()` draws the existing custom cheats grid above gameplay controls
- `TouchOverlayView.handleCheatsOverlayTouch()` consumes touches, closes the overlay, or calls `cheatCodeCallback`
- `MainActivity` wires `cheatCodeCallback` by sending each character through `nativeTextInput`
- The engine cheat logic remains in `d1/main/gamecntl.c` and `d2/main/gamecntl.c`, so Kotlin currently mirrors the cheat list in `TouchBindings.CHEATS_D1` and `TouchBindings.CHEATS_D2`

## Settings Tray Fit
- The requested menu is the bottom settings/admin tray, not the `More` unbound-actions overlay
- The tray action list is centralized in `AdminTrayPolicy.adminTrayVisibleActions()`
- Tray action constants live in `TouchOverlayView.Companion`
- Tray selection flows through `TouchOverlayView.handleAdminTrayTouch()` and `handleAdminTrayGamepadKey()`
- `MainActivity.syncAdminTrayPause(open = true)` already opens the single-player pause window using `nativeOpenSinglePlayerPauseIfSafe()`
- The native helper refuses to pause when not in live gameplay, when multiplayer is active, or when another menu is already front

## Recommended Shape
- Add a new admin action, for example `ADMIN_CHEATS = 24`
- Add it to `adminTrayVisibleActions()` for non-automap gameplay settings
- Prefer hiding it in multiplayer because engine cheats are ignored in `GM_MULTI` and the requested pause behavior is single-player only
- Add `adminTrayLabel(ADMIN_CHEATS) = "Cheats"`
- Make `adminTrayClosesAfterActivate(ADMIN_CHEATS)` return false, because it should open a tray-owned subpanel instead of closing the stack
- In `handleAdminTrayTouch()` and `handleAdminTrayGamepadKey()`, handle `ADMIN_CHEATS` like `ADMIN_DIFFICULTY`: open a child panel instead of dispatching to `adminTrayCallback`
- Keep cheat injection in `TouchOverlayView` through `cheatCodeCallback` unless there is a separate reason to add a native direct-cheat API

## Cheats List Panel
- Replace the existing grid overlay with a tray child state, for example:
  - `adminTrayCheatsMenuOpen`
  - `adminTrayCheatsPressedIndex`
  - `adminTrayCheatsPointerId`
  - `adminTrayCheatsScrollY`
  - `adminTrayCheatsRowRects`
  - `adminTrayCheatsBackRect`
  - `adminTrayCheatsPanelRect`
- Draw it after `drawAdminTrayPanel()` and after the difficulty popup, similar to `drawAdminTrayDifficultyMenu()`
- Put `Back` at the top, outside the scrollable region
- The scrollable list should show code and label side by side, with the code uppercased for display but injected in the existing lowercase form
- Touch handling should:
  - Close only the child panel when Back is tapped
  - Track drag vs tap so list scrolling does not accidentally activate a cheat
  - Clamp scroll offset against content height
  - Select a row only when the up event lands on the same row without meaningful drag
- Gamepad handling can initially support Back plus D-pad row selection if desired, but touch-only would satisfy the stated request

## Removal Work
- Remove `BTN_CHEATS_MENU` from `TouchBindings.BUTTON_LABELS` so it can no longer be selected in the touch editor
- Remove special handling from `pressLayoutButtonBinding()` and `releaseLayoutButtonBinding()`
- Remove old `cheatsOverlayOpen` drawing and touch consumption
- Remove old cheats overlay state and helper functions once the admin child panel is in place
- Update `dragZoneButtonLatchAllowed()` and its test because the cheats button will no longer be a special layout button
- Update `automapTouchButtonVisible()` tests if the old binding is removed from the visible-button universe
- Consider a layout migration from version 3 to 4 that drops buttons bound to the old value `100`

## Cheat Catalog Notes
- Current D2 Kotlin list omits lamer aliases from `d2/main/gamecntl.c`: `motherlode`, `currygoat`, `zingermans`, `eatangelos`, `ericaanne`, `joshuaakira`, and `whammazoom`
- Current D1 behavior requires `gabbagabbahey` before other D1 cheats work; simply selecting another D1 cheat first will be ignored by the engine
- `PIGFARMER` was not found as a cheat code in this repo; current D1 level warp is `farmerjoe`, and current D2 level warp is `freespace`
- A separate catalog cleanup may be useful before exposing this as a polished list, especially if the menu is expected to show every usable code

## Tests To Add Or Update
- `AdminTrayUiTest`: cheats action appears in single-player tray, is absent in multiplayer and automap, and does not close the tray directly
- `TouchOverlayDragZonePolicyTest`: remove or replace the old cheats-button special case
- `TouchLayoutRepository` migration test: legacy layout buttons with binding `100` are removed or safely ignored
- A focused JVM test for cheat-list layout math would be useful if the scroll calculations are factored into small helpers
- Manual or integration smoke: open settings tray in single-player, verify pause opens, open Cheats, scroll, Back closes child panel, selecting a cheat injects text and returns to gameplay or leaves the tray in the chosen final state

## Open Decisions
- Whether selecting a cheat should close the cheats child panel, the whole settings tray, or keep the list open
- Whether D1 should auto-inject `gabbagabbahey` before non-enabler cheats, or whether the list should expose the unlock requirement plainly by ordering and description
- Whether to show D2 lamer aliases, hide them, or group them under one description
- Whether to add a native cheat catalog/export API later so Kotlin does not mirror `gamecntl.c`

## Implementation Pass
- [x] Start implementation plan
- [x] Add settings tray Cheats action and child list
- [x] Remove old selectable overlay cheat button path
- [x] Add D1 auto-enabler injection behavior
- [x] Add layout migration and focused tests
- [x] Run focused verification

## Verification
- [x] `.\android\run-code-quality.ps1 -Fix -Paths ...` scoped to touched Kotlin files and the plan note
- [x] `.\gradlew.bat --no-daemon testDebugUnitTest --tests com.dxxredux.app.AdminTrayUiTest --tests com.dxxredux.app.TouchOverlayDragZonePolicyTest --tests com.dxxredux.app.GyroToggleConfigTest --tests com.dxxredux.app.TouchCheatInjectionTest`
