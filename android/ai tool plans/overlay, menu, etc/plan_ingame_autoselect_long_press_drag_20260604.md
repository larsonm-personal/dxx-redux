# In-game Autoselect Long-press/Drag Plan - 2026-06-04

## Goal
Make the in-game weapon autoselect ordering menus usable without Shift+Up/Down:

- Touch: long-press an item to grab it, drag vertically to reorder, release to drop in place.
- Controller: long-hold button A to grab the highlighted item, D-pad/stick up/down moves the grabbed item, A or B drops it in place.

This applies to both D1 and D2, with minimal, matching edits in `d1/` and `d2/`.

## Key Files

- `d1/main/weapon.c`, `d2/main/weapon.c`
  - `ReorderPrimary()` and `ReorderSecondary()` build the reorder menus.
  - Current subtitle is `Shift+Up/Down arrow to move item`.
  - These call `newmenu_doreorder()`.

- `d1/main/newmenu.c`, `d2/main/newmenu.c`
  - `struct newmenu` has `reorderitems`.
  - `newmenu_doreorder()` sets `menu->reorderitems = 1`.
  - `newmenu_key_command()` currently swaps rows only on `KEY_SHIFTED+KEY_UP` / `KEY_SHIFTED+KEY_DOWN`.
  - `newmenu_mouse()` currently supports normal tap selection, scroll-box drag suppression, and Android tap-outside-close.
  - `newmenu_handler()` maps Android joystick buttons to menu keys:
    - button 0 -> `KEY_ENTER`
    - button 1 -> `KEY_ESC`
    - buttons 22-25 -> D-pad arrows

- `d1/main/newmenu.h`, `d2/main/newmenu.h`
  - Exposes menu accessors used by automation/introspection.
  - May need a small accessor if tests need to inspect grabbed/reorder state.

- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
  - Maps Android gamepad A/B to virtual joystick buttons 0/1.
  - Sends D-pad as virtual joystick buttons 22-25.
  - Non-game menus receive face-button down/up through `nativeJoystickButton()`.

- `android/app/src/main/cpp/android_input.c`
  - Injects touch and virtual joystick events into SDL.
  - `nativeJoystickButton()` preserves button down/up events.
  - Touch coordinates are remapped through the menu-scale touch lock before SDL mouse events are pushed.

- `d1/arch/sdl/mouse.c`, `d2/arch/sdl/mouse.c`
  - Android touch motion is absolute-position mouse motion.
  - Mouse button down/up and movement already reach `newmenu_mouse()`.

- `d1/arch/sdl/event.c`, `d2/arch/sdl/event.c`
  - Android B fallback maps joystick button 1 to ESC for menu windows when unhandled.
  - Reorder-menu B handling should consume the event before this fallback closes the menu.

- `android/app/src/main/cpp/shared/game_introspect.cpp`
  - Serializes current menu title/subtitle/items/selected index/item bounds.
  - Already enough to verify final item order; optional grabbed-state exposure could make tests cleaner.

- `android/app/src/main/cpp/shared/game_automate.cpp`
  - `send_button` can hold and release controller buttons.
  - Existing touch automation only has `send_touch_tap`; touch drag/long-press coverage likely needs a small new action.

- `android/game_scripts/test_autoselect_crash_unified.json5`
  - Already navigates to the in-game primary autoselect menu.
  - Good place to extend controller reorder coverage or use as a template for a new focused test.

## Considerations

- Keep D1/D2 changes synchronized. `newmenu.c` is duplicated, and this behavior belongs in both copies.
- Avoid changing the launcher `AutoselectEditorPage.kt`; this bug is about the in-game menus.
- Keep desktop behavior intact. Existing `Shift+Up/Down` should still work.
- Gate new grab/drop behavior to `menu->reorderitems` so ordinary menus do not inherit long-hold or drag reordering.
- A normal A tap on reorder menus currently closes the menu because the rows are `NM_TYPE_MENU`. New behavior should not make quick A taps surprising unless deliberately chosen.
- B while grabbed should drop only, not close the menu. B while not grabbed can keep existing close behavior.
- Touch release should drop. Do not require a second tap after dragging.
- Dragging should reorder by item crossing, not pixel-perfect free movement; this matches the menu's row-based data model and keeps persistence simple.
- Touch autoscroll may be needed if dragging to the top/bottom of a scrollable reorder menu, especially D2 with 11 items on smaller layouts.
- The separator item `--- Never Autoselect below ---` is a real reorder item with value `255`; it must remain draggable.
- D1 primary has a special `Quad Lasers` value `16`; preserve it exactly.
- The menu-scale touch lock in `android_input.c` is important. Use existing remapped mouse coordinates rather than adding a parallel Kotlin-side hit test.

## Proposed Work Items

1. Add reorder-menu state to `struct newmenu`.
   - Suggested fields: grabbed index, original index if needed, pointer/touch start time, pointer id not needed for SDL 1.2 single-touch, and a flag for touch-grabbed versus controller-grabbed.
   - Initialize in `newmenu_do4()`.

2. Factor row swapping in `newmenu.c`.
   - Extract the existing Shift+Up/Down swap into a helper such as `newmenu_reorder_move(menu, direction)`.
   - Use this from old keyboard handling, controller grabbed movement, and touch drag movement.

3. Implement controller grab/drop.
   - On Android joystick button 0 down in a reorder menu, record press start time.
   - When the hold threshold is reached, grab the current item and consume the event.
   - While grabbed, D-pad/stick up/down should call the reorder move helper instead of normal focus movement.
   - Button 0 or 1 should drop the item in place and consume the event.
   - Decide whether hold detection is polled from draw/frame events or triggered by key repeat/button repeat. Polling is safer because `MainActivity` suppresses repeated face-button downs outside gameplay.

4. Implement touch long-press/drag.
   - On left button down in a reorder menu, remember selected item and press time.
   - If the finger remains down past the threshold without excessive movement, grab the item and give haptic/log feedback only if an existing path is available.
   - While grabbed and mouse moves over a different row, swap toward that row.
   - On left button up, drop in place and suppress normal menu activation/close.
   - Preserve existing drag-scroll behavior for non-reorder scroll boxes.

5. Update menu subtitles.
   - In both games, change the reorder subtitle to mention controller/touch behavior on Android.
   - Prefer `#ifdef ANDROID` strings in `weapon.c` so desktop keeps the old keyboard-specific help.

6. Optional introspection extension.
   - Add fields like `menu.reorder_grabbed_index` and `menu.reorder_active` only if tests need them.
   - Otherwise assert through item order and selected index to keep API surface smaller.

7. Test controller path first.
   - Add or extend a JSON5 game script:
     - Navigate to `Primary autoselect ordering...`.
     - Move focus to a known row.
     - Hold button 0 long enough to grab.
     - Send D-pad down/up while held/grabbed.
     - Press button 0 or 1 to drop.
     - Assert `menu.items[n].value` / text order changed and menu remains open.
   - Also cover B while grabbed dropping without closing.

8. Add touch automation only if feasible in this tranche.
   - Extend `game_automate.cpp` with touch down/move/up steps using existing `android_push_touch_action()` or a small exported helper.
   - Use introspected item bounds to choose row centers if the script system can consume dynamic positions. If not, use a fixed menu-scale stable layout or defer full touch automation to manual verification.

9. Verify.
   - Run focused Android game script via `android/helpers/run_test.ps1`.
   - Run at least the narrow unit/integration tests touched by automation changes.
   - Run `android/run-code-quality.ps1 --fix` after implementation.
   - Build both D1 and D2 Android targets, or at minimum the standard Android debug build used by the test helper.

## Open Design Questions

- Should a quick A tap in a reorder menu do nothing, close the menu, or drop only if already grabbed? The bug text only specifies long-hold A to select and A/B to deselect/drop, so safest behavior is: quick A keeps existing close behavior when not grabbed, A drops when grabbed.
answer: quick tap within the menu area should do nothing. outside the menu area should close
- Hold threshold: reuse an existing Android menu long-hold value if one exists near pilot delete behavior; otherwise use about 500-700 ms for touch and controller. Avoid the 2 second controller-config long-press threshold unless consistency with pilot delete is desired.
answer: 300-500ms should be the default if no android value can be loaded
- Visual feedback: the existing menu draws only the current row. If grabbed-state feedback is needed, simplest option is to keep selection on the grabbed row and alter prefix/text only under `menu->reorderitems`; anything more elaborate risks larger D1/D2 diffs.
answer: I want to see drag feedback during drags, and selection feedback. however you want to do that is ok

## Status

- [x] Planning survey complete.
- [x] Implementation complete for D1/D2 in-game reorder menus.
- [x] Controller automation added to `android/game_scripts/test_autoselect_crash_unified.json5`.
- [ ] Touch drag automation not added; implemented path is covered by Android native build and should be manually checked on device.
- [x] Verification run:
  - `android/gradlew.bat :app:assembleDebug`
  - `android/helpers/run_test.ps1 -ScriptName test_autoselect_crash_unified.json5 -Install -Game d2 -TimeoutSeconds 300`
  - `android/run-code-quality.ps1 -Fix -Paths @('android/game_scripts/test_autoselect_crash_unified.json5','android/ai tool plans/overlay, menu, etc/plan_ingame_autoselect_long_press_drag_20260604.md')`
