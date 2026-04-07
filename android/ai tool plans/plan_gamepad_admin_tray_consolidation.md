# Gamepad-Only Admin Tray Consolidation -- DONE

## Goal
When no touch interface is available (Android TV / gamepad-only), consolidate
standalone overlay buttons (Warp, Accept Join) plus music controls and
unreachable keyboard shortcuts (Automap, Headlight) into the admin tray grid.
Add D-pad navigation to the admin tray so it works without touch.

## Detection
- `!packageManager.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)` = gamepad-only
- Computed once in MainActivity.onCreate(), passed to TouchOverlayView

## Grid Layout (gamepad-only mode)
15 items, 5 clean rows of 3 columns:

| Row | Col 0 | Col 1 | Col 2 |
|-----|-------|-------|-------|
| 0 | View+ | View- | AutoLevel |
| 1 | Quick Save | Quick Load | Game Menu |
| 2 | Net Events | Exit | Net Stats |
| 3 | Video Info | Automap | Headlight |
| 4 | Warp | **Music** | Accept |

Row 4 is the "special" gamepad row. Middle = Music (always available, default
selection when tray opens via Start button).

Non-gamepad mode stays at 10 items (unchanged).

### Rationale for extra items
- **Automap (TAB)**: unreachable on gamepad, essential for navigation
- **Headlight (H)**: unreachable on gamepad, essential in dark levels
- **Music**: music panel trigger, requested by user
- **Warp**: coop warp to player, replaces WarpButtonOverlay
- **Accept**: accept join request, replaces AcceptJoinButtonView

## Button Mapping (gamepad-only, in-game)
- **Start**: toggle admin tray
- **Select**: send ESC (game pause menu)
- These override the normal mixer/meta routing for buttons 6 and 7

## D-pad Navigation (admin tray only, when open)
- D-pad left/right/up/down moves `adminTraySelectedIndex`
- A button activates selected item + closes tray
- B button closes tray
- All other gamepad buttons consumed (no game input while tray open)
- Selected item gets a highlight border (bright white/cyan)
- Default selection on open = middle of last row

## Dynamic Labels
- Warp: "Warp: [callsign]" when available, "Warp: --" when not
- Accept: "Accept: [callsign]" when pending, "Accept: --" when not
- These use polling providers similar to existing autoLevel/cockpitMode providers

## Files to Modify

### TouchOverlayView.kt
- [x] Add `var gamepadOnlyMode = false`
- [x] Add constants: ADMIN_AUTOMAP=10, ADMIN_HEADLIGHT=11, ADMIN_WARP=12, ADMIN_MUSIC=13, ADMIN_ACCEPT_JOIN=14
- [x] Add `adminTraySelectedIndex` state (-1 = no selection)
- [x] Modify `drawAdminTrayPanel()`: itemCount based on mode, selected highlight
- [x] Modify `handleAdminTrayTouch()` drag itemCount
- [x] Add labels for new items in `adminTrayLabel()`
- [x] Add providers: warpLabelProvider, acceptLabelProvider
- [x] Add `fun toggleAdminTray()` and `fun isAdminTrayOpen()` public methods
- [x] Add `fun handleGamepadKey(keyCode: Int, action: Int): Boolean`
- [x] Add `adminTrayDefaultSelection()` helper

### MainActivity.kt
- [x] Detect gamepad-only mode via PackageManager
- [x] Set `touchOverlay.gamepadOnlyMode`
- [x] Intercept Start/Select in onKeyDown/onKeyUp for admin tray
- [x] Route admin tray D-pad keys via touchOverlay.handleGamepadKey
- [x] Wire new ADMIN_ actions in adminTrayCallback
- [x] Add warp/accept label providers to touchOverlay
- [x] In gamepad-only mode: hide WarpButtonOverlay, AcceptJoinButtonView (not added to layout)
