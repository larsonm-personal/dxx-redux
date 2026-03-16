# Touch Interface Bug Fixes

8 touch interface issues: editor drag, button picker UI, gyro checkbox, gyro axis selection, map button overlay, automap gestures, radial segment editor, D2 stick reset.

## Phase 1: Touch Editor Drag Fix
**Problem:** Dragging a control stops when finger outruns it. `fingerDist <= maxDist` guard in TouchEditorPage.kt detectDragGestures (maxDist = controlRadius + 10% canvas width).
**Fix:** Remove the distance check. Compose drag gestures provide continuous deltas -- clamping is unnecessary.
**Files:** TouchEditorPage.kt ~L345-358

## Phase 2: Button Picker UI
**Problem:** "Standard"/"Extra" toggle is a plain TextButton.
**Fix:** "view extra buttons" / "view standard buttons" text with bright blue rounded outline (BorderStroke 2dp, #2196F3, RoundedCornerShape 8dp).
**Files:** TouchEditorPage.kt ~L1540-1552

## Phase 3: Gyro Checkbox Indicator
**Fix:** Add Checkbox next to "Gyro" in GyroSettingsDialog title, reflecting gyroConfig.enabled.
**Files:** TouchEditorPage.kt ~L1747-1820

## Phase 4: Gyro Axis Selection
**Problem:** GyroConfig has axisX/axisY but no UI to change them. Default "aim" (axes 2/3), user wants "slide" option (axes 0/1).
**Fix:** Radio buttons: "Aim (Turn/Pitch)" axisX=2/axisY=3, "Slide (L-R/U-D)" axisX=0/axisY=1.
**Files:** TouchEditorPage.kt ~L1747-1820

## Phase 5: Map Button Overlay Positioning
**Problem:** Map button uses player's configured position. Should be top-right when automap is active.
**Fix:** In computeGeometry(), when automap active, override to top-right (cx = w - radius - margin, cy = radius + margin).
**Files:** TouchOverlayView.kt ~L360-368

## Phase 6: Automap Touch Gesture Tuning
**Problem:** Two-finger rotation/zoom too slow, pan inverted.
**Fix (Kotlin-side handleAutomapTouch):** bankDelta *= 3.0, thrustDelta *= 3.0, negate vertical/sideways deltas.
**Files:** MainActivity.kt ~L768-860

## Phase 7: Radial Menu Segment Editor
**Problem:** "Add Radial" creates 3 dummy segments, no editing UI. Presets (PriWpn/SecWpn/Guide) stay read-only.
**Design:**
- Add `bindingType` to RadialSegment ("keycode" for presets, "action" for custom). Default "keycode" for JSON backwards compat.
- Custom segments use button IDs (0-54) / meta IDs (1000+), reuse ButtonBindingPicker.
- Segment editor UI in radial properties panel: label + binding picker + delete per segment, "Add Segment" button.
- Preset radials (PriWpn/SecWpn/Guide) show "Preset -- not editable".
**Files:** TouchControl.kt ~L248-269, TouchEditorPage.kt ~L1265-1283, TouchOverlayView.kt ~L1449-1478

## Phase 8: D2 Stick Reset Bug
**Problem:** After Full Reset, left stick controls look instead of slide/throttle, right stick does nothing.
**Analysis:** Reset patches .plr with correct 4-axis overrides via nativeResetToDefaults, then kills process. Likely: process killed before filesystem flush, or engine init ordering issue.
**Fix:** Add fsync after .plr patching, verify writeDefaultControllerConfig values, check engine init ordering.
**Files:** android_gamepad_config.cpp, SetupActivity.kt ~L2998-3020

## Status
All 8 phases implemented, builds verified (D1 ninja, D2 ninja, Android APK), code quality linters pass.
