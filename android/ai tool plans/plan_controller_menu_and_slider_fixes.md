# Controller + Slider Round 2 Fixes

Addresses four regressions reported after the last build:

1. MIDI preview progress slider traps D-pad up/down, blocking vertical menu exit
2. In-game menus do not respond to controller A (select) or B (back)
3. In-game menus do not respond to controller D-pad
4. Some controller sequence opens the music tracks overlay from the main menu with no way to close it

Working buttons before this change were only "any button to skip movies" and the TV remote Back gesture.

## Root cause summary (from Explore subagent)

- `MainActivity.dispatchDpad()` and the `joyBtn >= 0` branch in `onKeyDown`/`onKeyUp` fell into an `if kcIndices != null -> mixer else -> nativeJoystickButton` structure. With the default controller config binding A, DUp, DDown, DLeft, DRight to gameplay actions, `mixerButtonMap` is always populated for buttons 0, 1 and 22-25, so `nativeJoystickButton` was never called. The ANDROID block in `d1/d2 newmenu.c EVENT_JOYSTICK_BUTTON_DOWN` translates those specific buttons to KEY_ENTER / KEY_ESC / KEY_UP / KEY_DOWN / KEY_LEFT / KEY_RIGHT, so the menu never saw input
- `MusicControlPanel` is a plain View that only overrides `onTouchEvent`. With no key path it could not be dismissed on TV. The admin tray default selection is ADMIN_MUSIC, so a START + A combo from the gamepad opens the overlay and leaves it stuck
- Material3 `Slider` consumes D-pad up/down for large-step adjustments, preventing focus traversal out of the slider on TV

## Fix plan

- [x] `dispatchDpad`: always call `nativeJoystickButton(btnIdx, ...)` for D-pad virtual buttons 22-25 in addition to the mixer. Meta-action rebinds still win (explicit user override)
- [x] `onKeyDown` / `onKeyUp` gamepad-button branch: always call `nativeJoystickButton(joyBtn, ...)` alongside the mixer so menus react to A/B. Kconfig does not bind the raw joy buttons during gameplay (launcher routes through keyboard via the mixer) so this is a no-op in-game
- [x] `MainActivity.onKeyDown`: if `musicPanel != null` and the key is BACK / ESCAPE / BUTTON_B / BUTTON_Y, call `dismissMusicPanel()` and consume. `onKeyUp` swallows the matching release
- [x] `MusicPickerPage`: add `verticalDpadFocusEscape()` modifier that handles D-pad up/down via `LocalFocusManager.moveFocus(...)` so focus leaves the slider. Applied to all three preview sliders (MIDI, CD, audio files)
- [x] Focused Kotlin compile: `.\gradlew.bat :app:compileDebugKotlin` from `android/`
- [x] Code quality: `.\android\run-code-quality.ps1 -Fix` from repo root

## Notes for upstreaming / portability

- All changes are in `android/` Kotlin except zero edits in `d1/` or `d2/`. The menu-side ANDROID block in `newmenu.c` was already present and is what this fix lights up
- The mixer / meta / joystick layering is documented at the top of `d2/arch/sdl/joy.c` and in `DpadFocusUtils.kt`. No new constants introduced
