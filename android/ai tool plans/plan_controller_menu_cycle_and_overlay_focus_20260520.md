# Controller Menu Cycle + Overlay Focus Plan

## Goal
Implement a bindable controller `Menu` action that gives controller-only access to the Android overlay surfaces used for unbound actions and in-game runtime settings.

Requested behavior:
- first press opens the extra menu for remaining unbound actions
- second press closes the extra menu and opens the settings/admin tray overlay
- third press closes the settings overlay, including any open child surface
- while any of these menus are open, D-pad plus A/B should control the menu instead of gameplay
- on touchless Android, the controller config page should show a bold red `No menu button` warning when nothing is bound to `Menu`
- the settings overlay must still open when no explicit settings button is placed in the touch layout

## Current Behavior From Code Study [x]
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt` has `META_GAME_MENU`, but no overlay menu-cycle action
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` hard-codes `Select` and `Start` behavior through `shouldUseControllerSettingsTrayShortcuts(...)`
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` already supports D-pad/A/B navigation for the admin tray, but not for the remaining-actions extra menu
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` uses an optional `DiagnosticType.SETTINGS` button, and bundled touch layouts can omit it entirely
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` gives `MoreActionsControl` a built-in default position, but settings diagnostics have no equivalent fallback
- `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt` and `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt` are touch-only, so some settings items are not yet controller-complete

## Root Design Decision
Treat this as an Android overlay-navigation feature, not as a native C meta-key dispatch feature.

That means:
- add a new Kotlin-side meta action ID for `Menu`
- intercept it in `MainActivity.dispatchMetaAction(...)`
- route it into a shared overlay controller state machine owned by `TouchOverlayView` plus small child-overlay handlers
- keep `Game Menu (ESC)` as a separate action because opening the native pause/menu stack and cycling Android overlays are different jobs

This also lets the fix stay entirely in `android/`, which matches the project preference to avoid unnecessary `d1/` and `d2/` edits.

## UX Behavior

### Default bindings
- `Select` -> `Menu`
- `Start` -> `Game Menu (ESC)`
- `Automap` remains reachable from the settings/admin tray in touchless mode

### Menu-cycle presses
- `none -> extra menu`
- `extra menu -> settings root`
- `settings root or any settings child -> closed`

### Button behavior while overlay menus are open
- D-pad moves the current menu focus
- A activates the current item
- B closes the current root menu, or closes a child surface and returns to the settings root

### Child surfaces that count as settings submenus
- music track picker
- video info overlay

The requested `B` behavior maps cleanly to these child overlays: B should close the child surface and restore focus to the admin tray root instead of dropping straight back to gameplay.

## Implementation Plan

### Phase 1: Binding model and controller-config warning
- [x] Add `META_MENU_CYCLE` and the label `Menu` to `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt`
- [x] Intercept `META_MENU_CYCLE` in `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` instead of forwarding it to `NativeMetaActions`
- [x] Update `android/app/src/main/assets/configs/controller/default.json` so fresh/default configs bind `Select` to `Menu` and `Start` to `Game Menu (ESC)`
- [x] Add a touchless-only warning helper in `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt`
- [x] Show bold red `No menu button` near the existing assignment summary when `!FEATURE_TOUCHSCREEN && !hasMenuBinding`
- [x] Remove or narrow the hard-coded Start/Select shortcut path once the binding-driven path is in place

Preferred end state:
- no special Start/Select routing outside the normal controller binding system

### Phase 2: Shared overlay menu state in `TouchOverlayView`
- [x] Add a small controller-visible menu-surface enum, for example:
  - `NONE`
  - `REMAINING_ACTIONS`
  - `ADMIN_TRAY_ROOT`
- [x] Add public helpers such as:
  - `cycleControllerMenu()`
  - `closeControllerMenu()`
  - `handleControllerMenuKey(keyCode, action): Boolean`
- [x] Extend remaining-actions state with `remainingActionSelectedIndex`
- [x] Reuse existing `adminTraySelectedIndex` for settings-root focus
- [x] Ensure opening one surface always closes the other so the three-press cycle is deterministic
- [x] Recompute geometry before controller navigation so layouts with omitted touch buttons still get valid default positions

### Phase 3: Fallback settings activator when no settings button exists
- [x] Reuse the existing bottom-center admin tray tab as the implicit settings activator when `DiagnosticType.SETTINGS` is absent
- [x] Allow that fallback activator path to exist in gamepad-only mode for controller-driven settings access
- [x] Do not require layout migrations or a mandatory settings diagnostic in bundled touch JSON
- [x] Keep explicit settings diagnostics authoritative when present

Status note:
- confirmed by current `TouchOverlayView` behavior; no additional code change was needed in this tranche

Why this shape:
- bundled layouts already prove that the settings button is optional
- the bottom-center admin tray tab already exists as the non-diagnostic fallback in touch mode
- reusing that tab is smaller and cleaner than introducing a second default-placement system

### Phase 4: Controller navigation for the extra menu
- [x] Add D-pad selection movement for remaining actions in `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- [x] A activates the highlighted remaining action
- [x] B closes the extra menu
- [x] Keep the extra menu as the controller-only path to unbound actions such as automap/headlight/guidebot-style items when they are not on a physical binding

### Phase 5: Controller navigation for settings child surfaces
- [x] `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt`
  - add selected-row state
  - D-pad up/down moves and scrolls the list
  - A plays the selected track
  - B closes the panel and returns to admin tray root
- [x] `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`
  - add a stable focus order across its interactive controls
  - D-pad moves focus
  - A activates the current control
  - B closes the overlay and returns to admin tray root
- [x] Keep display-only overlays such as net stats and net events as root-level toggles unless a later tranche needs deeper controller control

### Phase 6: MainActivity routing cleanup
- [x] Centralize overlay-controller routing in `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- [x] When any controller overlay surface is active, consume D-pad/A/B before mixer or native gameplay dispatch
- [x] Route priority as:
  1. settings child surface
  2. admin tray root
  3. remaining-actions root
  4. normal mixer/meta/native dispatch
- [x] Define menu-cycle behavior on settings child surfaces as `close the whole settings stack`

### Phase 7: Tests and validation
- [x] Add JVM tests for pure helper logic:
  - cycle-state transitions
  - touchless warning predicate
  - default controller asset contains `Menu`
  - remaining-actions/admin-tray helper behavior
- [x] Extend `android/app/src/test/java/com/dxxredux/app/AdminTrayUiTest.kt`
- [x] Add focused unit coverage for child-surface key handlers where the logic can be kept pure
- [x] Run targeted Android unit tests and a focused debug build
- [x] Run `android\run-code-quality.ps1 -Fix`
- [x] Run `:app:externalNativeBuildDebug`
- [ ] Manual validation on a touchless emulator or Shield-class device:
  - bind/unbind `Menu` and verify the warning
  - first/second/third press cycle
  - D-pad/A/B in extra menu
  - D-pad/A/B in settings root
  - D-pad/A/B in music panel
  - D-pad/A/B in video info overlay

### Phase 8: Close-path and focus polish
- [x] Treat the settings root tray as a close-on-`Menu` surface so the cycle stays `game -> more -> settings -> game`
- [x] Keep `B` as the explicit close action for both the More menu and the settings root tray
- [x] Keep D-pad plus `A`/`B` routed to overlay navigation before gameplay dispatch while either root menu is open
- [x] Draw green controller-focus outlines for both the More menu and the settings root tray
- [x] Extend the focused JVM helper coverage for the settings-root close case

### Manual follow-up 2026-05-21
- [x] Investigate the touchless follow-up where focus outlines appeared but controller D-pad still did not move overlay focus and `Menu` could not close the settings tray
- [x] Fix HAT-based D-pad routing so controller D-pad motion reaches overlay handlers before native/game dispatch
- [x] Fix button-meta routing so a bound `Menu` button is not swallowed by the admin tray before `META_MENU_CYCLE` dispatch
- [x] Re-run focused JVM validation with JDK 21 after the routing fix

Validated result:
- `MainActivity.dispatchDpad(...)` now checks controller settings children and `TouchOverlayView.handleControllerMenuKey(...)` before forwarding HAT D-pad events to native joystick/menu paths
- `MainActivity.onKeyDown()` / `onKeyUp()` now allow a bound `META_MENU_CYCLE` gamepad button to bypass overlay swallowing so the cycle can close the settings tray as designed
- Focused JVM validation passed with `android\\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ControllerMenuCycleTest --tests com.dxxredux.app.AdminTrayUiTest --tests com.dxxredux.app.SettingsChildOverlayControllerTest` after setting `JAVA_HOME=C:\\local\\jdk-21`

## Likely File Set
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt`
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt`
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`
- `android/app/src/main/assets/configs/controller/default.json`
- `android/app/src/test/java/com/dxxredux/app/AdminTrayUiTest.kt`
- new focused JVM tests under `android/app/src/test/java/com/dxxredux/app/`

## Notes and Non-goals
- No `d1/` or `d2/` engine edits should be needed for this tranche
- Keep `Game Menu (ESC)` separate from `Menu`
- Do not push missing-settings placement into bundled JSON; the fallback position is an Android runtime concern
- This plan supersedes the old hard-coded Start/Select shortcut behavior described by `android/ai tool plans/plan_gamepad_admin_tray_consolidation.md`
- If later automation needs stronger coverage, add a tiny Android-side debug status hook for the current overlay controller surface instead of trying to infer it from screenshots

## Phase Status
- [x] Design study and owner/file mapping
- [x] Phase 1 implementation and validation
- [x] Phase 2 groundwork implementation and validation
- [x] Phase 3 fallback activator audit and validation
- [x] Phase 4 extra-menu controller navigation and validation
- [x] Phase 5 child-surface controller navigation and validation
- [x] Phase 6 menu-stack behavior and validation
- [x] Phase 8 close-path and focus polish
- [ ] Full manual controller validation