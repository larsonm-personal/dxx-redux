# Fix: Multi-Item Menu Keyboard Viewport Centering

## Problem
1. In menus with mixed item types (e.g., Options > Graphics Options), the keyboard opens immediately on menu activation even when the selected item (`citem`) is NOT an input field
2. The viewport shift always targets the FIRST input field found in the menu, not the currently selected item
3. `g_active_input_field_y` never updates when the user navigates between items, so the viewport is stuck centering on the wrong item
4. Drag-to-scroll changes `scroll_offset` which affects visible positions, but field_y doesn't track

## Root Cause
In `EVENT_WINDOW_ACTIVATED` (both d1/ and d2/ newmenu.c), the code loops through ALL items looking for ANY `NM_TYPE_INPUT` or `NM_TYPE_INPUT_MENU`, then calls `android_show_keyboard()` with the first one's Y position. It ignores `menu->citem` entirely.

## Fix

### 1. android_input.c: add `android_update_keyboard_field_y()`
- New function that sets `g_active_input_field_y` without showing/hiding the keyboard
- Used for per-frame tracking of the selected item's visible position

### 2. d2/main/newmenu.c + d1/main/newmenu.c: fix EVENT_WINDOW_ACTIVATED
- Only show keyboard if `menu->items[menu->citem]` is `NM_TYPE_INPUT` or `NM_TYPE_INPUT_MENU` with `group==1`
- Use `citem`'s actual visible Y (accounting for scroll_offset)

### 3. d2/main/newmenu.c + d1/main/newmenu.c: per-frame field_y update in newmenu_draw
- After drawing items, compute `citem`'s visible screen Y: `menu->y + item.y - LINE_SPACING * scroll_offset`
- Call `android_update_keyboard_field_y(visible_y)` every frame
- This handles ALL selection change sources (keys, mouse clicks, drag-to-scroll) in one place

## Y coordinate calculation
- `menu->items[i].y` = item Y within the menu canvas (assumes scroll_offset=0)
- Visible canvas Y = `item.y - LINE_SPACING * scroll_offset`
- Absolute screen Y = `menu->y + item.y - LINE_SPACING * scroll_offset`
- `g_active_input_field_y` operates in screen coordinates (compared against `canvas_h` in `android_get_keyboard_y_offset`)

## Files changed
- `android/app/src/main/cpp/android_input.c` - add `android_update_keyboard_field_y()`
- `d2/main/newmenu.c` - fix EVENT_WINDOW_ACTIVATED, add per-frame update in newmenu_draw
- `d1/main/newmenu.c` - same changes as d2
