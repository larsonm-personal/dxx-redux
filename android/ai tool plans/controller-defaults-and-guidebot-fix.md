# Controller Defaults & Guidebot Fix

## Changes Made

### 1. Left stick default: Pitch U/D (was Slide U/D)
- **File**: `ControllerConfigPage.kt` — `DEFAULT_BINDINGS`
- Changed `"LS_Y" to "Slide U/D"` → `"LS_Y" to "Pitch U/D"`
- Left stick Y now defaults to forward/backward pitch, matching expected FPS controls

### 2. AB button default: Afterburner (was unset → Fire Primary)
- **File**: `ControllerConfigPage.kt` — `DEFAULT_BINDINGS`
- Added `"Y" to "Afterburner"` entry
- Y button now defaults to afterburner instead of being unset

### 3. Guidebot radial menu fix
- **File**: `TouchOverlayView.kt`
- **Problem**: Guidebot radial sent raw digit keys (KEYCODE_1-9) which only work inside the escort menu window. Outside that window, digits select weapons (e.g. "Exit"/9 selects Mega missile).
- **Fix**: When firing a Guide radial selection, inject the full Shift+F4 key sequence to open the escort menu first, then send the digit key:
  1. SHIFT_LEFT keydown
  2. F4 keydown (key_handler sees LSHIFT pressed → adds KEY_SHIFTED → `do_escort_menu()` opens)
  3. F4 keyup
  4. Digit keydown (routed to escort menu window → `set_escort_special_goal()`)
  5. Digit keyup
  6. SHIFT_LEFT keyup
- **D1 handling**: Guide radial is hidden (not drawn, not touchable) when `gameVariant == "d1"` since D1 has no Guide-Bot
- **D2 buddy checks**: The game engine (`do_escort_menu()`) already handles all edge cases: multiplayer, no buddy in mine, buddy not released — shows appropriate HUD messages

### 4. Button binding picker scroll indicators
- **File**: `TouchEditorPage.kt` — `ButtonBindingPicker`
- **Problem**: After the nested-scroll crash fix, the DropdownMenu lost its scroll indicators
- **Fix**: Replaced `DropdownMenu` with `AlertDialog` containing a scrollable `Column` + `ScrollArrows`. This avoids the nested scrollable container crash while providing proper scroll indicators. Current binding is highlighted in primary color.

## Key event flow (guidebot)
```
nativeKeyEvent(Kotlin) → SDL_PushEvent (C) → event_poll → key_handler
  → keyd_pressed[KEY_LSHIFT]=1
  → F4 gets KEY_SHIFTED flag → gamecntl case KEY_F4+KEY_SHIFTED → do_escort_menu()
  → escort menu window created → next event (digit) routed to it
  → escort_menu_keycommand → set_escort_special_goal(key & ~KEY_SHIFTED)
```
