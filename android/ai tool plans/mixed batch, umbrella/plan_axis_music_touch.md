# Plan: Axis-button fix, music fix, touch editor improvements

## Issue 1: RT fire primary broken (axis-to-button mismatch)

### Root cause
When RT is bound to Fire Primary (a button function), the flow is:
1. Kotlin `onGenericMotionEvent` -> `inputMixer.setAxis(5, "ctrl", rt)` -> JNI `nativeJoystickAxis(5, value)`
2. C `joy_axisbutton_handler` detects threshold crossing -> generates button 21 (RT positive axis-button)
3. C kconfig: `kc_joystick[0].value == 21?` -- NO, value is 100 (identity mapped MIXER_BTN_BASE + 0)
4. Fire Primary never fires

Face buttons work because onKeyDown calls mixer which sends MIXER_BTN_BASE + kc.
Axes don't go through mixer for button conversion.

### Fix
Add Kotlin-side axis-to-button conversion in `onGenericMotionEvent`:
- Build an axisButtonMap from controller_config.json: for each axis (0-5), collect which kc_joystick button indices are bound to axis-button SDL values (10-21)
- When axis crosses threshold, call `inputMixer.setButton(kcIdx, tag, pressed)`
- The mixer then sends `MIXER_BTN_BASE + kcIdx` which matches identity mapping

Files to change:
- [x] ControllerConfigPage.kt: add `axis_button_bindings` to config JSON (axis index -> list of (kcIdx, isPositive))
- [x] MainActivity.kt: load axis_button_bindings, add threshold-comparison in onGenericMotionEvent
- [x] The C-side axis-button handler still runs but generates unmatched button events (harmless)

## Issue 2: Music sets (jukebox) not playing

### Root cause
M3U entries contain absolute paths like `/data/user/0/com.dxxredux.app/files/custom_music/set1/track.mp3`.
`mix_play_file()` in `digi_tsf_music.c` calls `PHYSFS_openRead(filename)` which can't handle absolute paths.
Desktop's `digi_mixer_music.c` uses `Mix_LoadMUS()` which handles absolute paths directly.

### Fix
In `mix_play_file()`, detect absolute paths (starts with `/`) and use `fopen()` instead of `PHYSFS_openRead()`.

Files to change:
- [x] android/app/src/main/cpp/shared/digi_tsf_music.c: add fopen fallback for absolute paths in PCM decoder path

## Issue 3: 5th double-tap mode (hold while touching)

Add HOLD_FIRE mode: double-tap starts firing, releasing second finger stops.

Files to change:
- [x] TouchControl.kt: add HOLD_FIRE to DoubleTapMode enum
- [x] TouchOverlayView.kt: add HOLD_FIRE handling in handleDoubleTap + release on finger lift
- [x] TouchEditorPage.kt: add HOLD_FIRE label in modeLabels map

## Issue 4: Standardize axis labels to yaw/roll/pitch

Replace all user-visible X/Y/Z axis references with game function names (Yaw, Pitch, Roll, etc.)

Files to change:
- [x] TouchEditorPage.kt: change "X Axis", "Y Axis", "Invert X", "Invert Y", "Sensitivity X", "Sensitivity Y" etc.
  to use axis label names from AXIS_LABELS

## Issue 5: Fixed names for touch joystick/slider/axis regions

Replace editable text id labels with fixed descriptive names based on bound axes.
For two-axis controls: both axes on two lines like "slide u/d\nturn l/r"
For single-axis: just the axis name

Files to change:
- [x] TouchOverlayView.kt: drawStick - add axis label, drawSlider - use axis label, drawAxisRegion - use axis label

## Build and test
- [x] Build APK - BUILD SUCCESSFUL
- [x] Run code quality - clang-format fixed, ktlint issues are pre-existing in BuildInfo.kt
- [x] Gyro dialog X/Y/Z -> Yaw/Roll/Pitch labels
