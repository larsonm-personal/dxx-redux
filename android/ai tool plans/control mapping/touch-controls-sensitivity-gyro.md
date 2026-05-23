# Plan: Touch Controls -- Double-Tap Fix, Sensitivity Refactor, Gyro Overhaul + Diagnostic

## Summary
Four interrelated touch control improvements:
1. Fix double-tap "fire primary" not firing (secondary works fine)
2. Split joystick sensitivity into separate X/Y, unify mouse/stick sensitivity
3. Overhaul gyro sensor to use TYPE_GAME_ROTATION_VECTOR with all 3 axes (yaw/pitch/roll)
4. Add optional draggable gyro diagnostic overlay showing real-time axis values

---

## Phase 1: Fix Double-Tap Fire Primary
**Status: DONE**

### Root Cause
- Double-tap sends press+release back-to-back in same Kotlin callback chain
- Both SDL events are drained in one event_poll() cycle (same game frame)
- game.c:1715 does `Global_laser_firing_count = Controls.fire_primary_state ? ... : 0` (direct assignment)
- If state is already cleared by the release, count = 0 and nothing fires
- gamecntl.c:256 uses `Global_missile_firing_count += ...` (accumulation) so secondary fire survives

### Fix
Kotlin-side latch: delay double-tap button release by ~50ms via Handler.postDelayed()

### Files
- TouchOverlayView.kt: three double-tap sites (~L1018, ~L1041, ~L1054) -- add delayed release

---

## Phase 2: Sensitivity Refactor
**Status: DONE**

### Changes
- Replace `sensitivity: Float` with `sensitivityX: Float` / `sensitivityY: Float` in AnalogStickControl
- Remove `mouseSensitivity: Float` -- mouse and stick modes share the same X/Y settings
- Add internal `MOUSE_BASE_MULTIPLIER` that stacks with sensitivity so mouse mode has a good default feel
- JSON migration: read old `sensitivity` into both X/Y; read old `mouseSensitivity` and discard

### Files
- TouchControl.kt: AnalogStickControl data model, toJson/fromJson
- TouchOverlayView.kt: updateStickFromTouch (~L1473), drainMouseBuffers (~L215), updateStickFromMouseDrag (~L1421)
- TouchEditorPage.kt: replace single slider with X/Y pair, remove mouse sensitivity slider
- HumanReadableConfig.kt: parseStick, stickToHuman

---

## Phase 3: Gyro Sensor Overhaul
**Status: DONE**

### Changes
- Switch from TYPE_ROTATION_VECTOR to TYPE_GAME_ROTATION_VECTOR (no magnetometer, less noise)
- Use SensorManager.getOrientation() for canonical yaw/pitch/roll extraction (instead of manual matrix math)
- Expose all 3 axes: yaw, pitch, roll (user can assign each to any game axis)
- GyroConfig grows axisZ (roll axis) + sensitivityZ + invertZ

### Files
- GyroInputManager.kt: sensor type, angle extraction, 3-axis output
- TouchControl.kt: GyroConfig data class -- add axisZ, sensitivityZ, invertZ
- TouchEditorPage.kt: GyroSettingsDialog -- add 3rd axis config, update axis mode presets
- HumanReadableConfig.kt: gyroToHuman, parseGyro -- add Z axis

---

## Phase 4: Gyro Diagnostic Overlay
**Status: DONE**

### Design
- New DiagnosticControl data class in TouchControl.kt
- Renders a small transparent box showing yaw/pitch/roll as -100% to +100%
- Updated in real-time from GyroInputManager
- No touch input (visual only, pass-through)
- Draggable in touch editor like any other control
- Works in both editor preview and in-game

### Files
- TouchControl.kt: DiagnosticControl data class, toJson/fromJson
- TouchOverlayView.kt: store latest gyro values, render diagnostic box
- TouchEditorPage.kt: add "Diagnostic Display" to AddControlDialog
- GyroInputManager.kt: add diagnosticCallback for raw pre-clamp values
- HumanReadableConfig.kt: serialization
- TouchLayout: add diagnostics list

---

## Verification
1. Double-tap: configure fire primary double-tap, verify it fires. verify secondary still works
2. Sensitivity: verify X/Y sliders appear, old configs migrate, mouse mode responsive
3. Gyro: verify TYPE_GAME_ROTATION_VECTOR, all 3 axes output sensible values via diagnostic overlay
4. Diagnostic: add control in editor, verify live-updating in editor and in-game
5. Code quality: run android/run-code-quality.ps1 --fix
6. Build: gradle assembleDebug
