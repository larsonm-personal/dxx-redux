# Five Fixes Plan

## Status: COMPLETE

## 1. Axis region fires primary/secondary at extremes (+ gyro axes bound to buttons when disabled)
**Root cause**: `joy_init()` uses `memset(&Joystick, 0, ...)` which zeros `axis_button_map[]`. The loop only sets entries 0-5, leaving 6 (AXIS_BANK) and 7 (AXIS_SLIDE_UD) as 0. `joy_axisbutton_handler()` looks up `axis_button_map[7] = 0`, treating it as button 0 (fire primary) with button 1 (fire secondary).

**Fix** (d1/arch/sdl/joy.c + d2/arch/sdl/joy.c):
- Set `axis_button_map[6]` and `[7]` to `-1` after the axis-button registration loop
- Add early `return 0` in `joy_axisbutton_handler()` when `button < 0`
- This also fixes issue #4 (gyro axes mapped to back/fire buttons when disabled)

## 2. SetupActivity.kt compiler warnings
- Removed redundant `.toInt()` on `0x7FFFFFFE` (already Int literal)
- Changed `safManifest?.pruneStaleEntries()` to `.pruneStaleEntries()` (non-null val)
- Removed `!!` on `gogDiscUri` and `instDiscUri` inside null-checked block (smart cast)

## 3. Gyro recenter persisted to config + editor live update
- Added `refAzimuth`, `refPitch`, `refRoll` (Float?) to GyroConfig with JSON serialization
- Added `onCalibrated` callback to GyroInputManager, fired when new reference established
- `setConfig()` loads persisted ref values into `refOrientation` if available
- MainActivity wires callback to save updated layout on each calibration
- Editor DisposableEffect wires callback to update layout state (triggers recomposition)
- GyroSettingsDialog shows "Reset" button when saved calibration exists

## 4. (Same root cause as #1, fixed together)

## 5. Axis region full-range from touch point
**Problem**: Symmetric `halfRange = (bottom-top)/2` meant touching near an edge limited throw in that direction.

**Fix**: Asymmetric scaling in `updateAxisRegionFromTouch()`:
- `posRange = touchOrigin to bottom edge`, `negRange = touchOrigin to top edge`
- Positive delta maps 0..posRange to 0..+1
- Negative delta maps 0..-negRange to 0..-1
- Full range available regardless of initial touch position

## Files changed
- d1/arch/sdl/joy.c -- axis_button_map sentinel, handler guard
- d2/arch/sdl/joy.c -- axis_button_map sentinel, handler guard
- SetupActivity.kt -- 3 warning fixes
- TouchControl.kt -- GyroConfig ref fields, toJson/fromJson
- GyroInputManager.kt -- onCalibrated callback, setConfig loads ref, fire on calibrate
- MainActivity.kt -- wire onCalibrated to save layout
- TouchEditorPage.kt -- editor gyro DisposableEffect wires onCalibrated, dialog ref state + reset button
- TouchOverlayView.kt -- asymmetric axis region scaling
