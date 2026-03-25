# Plan: Gyro Overhaul, Touch Drag Fix, GL Error Suppression (Build 1024)

## Summary
Seven related changes: fix gyro diagnostic not updating live in the touch editor, add recenter-on-tap for gyro recenter buttons in the editor, swap pitch/roll display labels for landscape, remove sensitivity and split tilt range per axis, replace bundled gyro axis presets with independent per-axis binding, fix single-axis vs joystick drag capture priority, and investigate/suppress GL error spam from the emulator.

## Phase 1: Fix Gyro Diagnostic Not Updating in Touch Editor
- [x] Root cause: DisposableEffect(gyroConfig) restarts GyroInputManager on every layout.gyro change, including ref value updates from onCalibrated, creating an infinite restart loop
- [x] Change DisposableEffect key to stable fields only (diagnostics presence + enabled)
- [x] Add LaunchedEffect to push config changes to existing manager via setConfig()
- [x] Buffer onCalibrated ref values locally, write to layout only on save
- [x] Fix resume() to preserve hasReference when setConfig loaded persisted refs
- Files: TouchEditorPage.kt L86-108, GyroInputManager.kt L68-69

## Phase 2: Gyro Recenter Button in Touch Editor
- [x] Detect taps on BTN_GYRO_RECENTER buttons in editor's tap/selection handler
- [x] Call editorGyroManager.calibrate() on such taps
- [x] Wire onCalibrated to persist ref values to config
- Files: TouchEditorPage.kt (Canvas pointerInput block)

## Phase 3: Swap Pitch/Roll Display Labels for Landscape
- [x] Diagnostic drawing: sensor "Pitch" shown as "Roll", sensor "Roll" shown as "Pitch"
- [x] Apply in both TouchEditorPage.kt and TouchOverlayView.kt
- [x] Settings dialog axis labels updated to match
- Files: TouchEditorPage.kt L1031-1033, TouchOverlayView.kt ~L1370

## Phase 4: Remove Sensitivity, Split Tilt Range Per Axis
- [x] GyroConfig: remove sensitivityX/Y/Z, replace maxAngle with maxAngleX/Y/Z
- [x] GyroInputManager: scaling = dAxis / (maxAngleAxis - deadzone) for both modes
- [x] GyroSettingsDialog: remove sensitivity sliders, add three tilt range sliders
- [x] JSON migration: old maxAngle -> all three, old sensitivity ignored
- Files: TouchControl.kt L405-475, GyroInputManager.kt L119-145, TouchEditorPage.kt L2510-2740, HumanReadableConfig.kt

## Phase 5: Independent Per-Axis Gyro Binding
- [x] Remove preset radio buttons (Aim/Slide/Roll+Slide) and "Roll Axis (3rd axis)" section
- [x] Add three independent AxisPicker dropdowns (Yaw/Roll/Pitch with landscape-swapped display names)
- [x] Keep GyroConfig.axisX/Y/Z fields unchanged; just change UI
- Files: TouchEditorPage.kt L2596-2668, TouchBindings.kt L193-202

## Phase 6: Fix Single-Axis vs Joystick Drag Capture Priority
- [x] Add if(!handled) guard before stick loop in onTouchEvent
- [x] When pointer leaves axis region where stealSourceStick==null, transfer to stick using default center
- [x] Existing steal-from-stick return logic preserved
- Files: TouchOverlayView.kt L1205 (guard), L1425-1460 (transfer logic)

## Phase 7: Investigate and Suppress GL Error Spam
- [x] Confirm source: emulator GLES translation layer logging 0x502 from matrix stack calls
- [x] Replace GLES 1.x matrix-stack math with CPU-side equivalents for projection/ortho setup in ogl_start_frame/ogl_end_frame/gr.c
- [x] Keep per-object transforms (crosshair, sphere) for later GLES 2.0 migration
- [x] Apply to both d1/ and d2/
- Files: d2/arch/ogl/ogl.c, d2/arch/ogl/gr.c, d1/arch/ogl/ogl.c, d1/arch/ogl/gr.c

## Phase 8: Lint, Build, Test
- [x] run-code-quality.ps1 --fix (only pre-existing BuildInfo.kt issues remain)
- [x] gradlew assembleDebug (BUILD SUCCESSFUL, no new warnings)
- [ ] Manual emulator verification
- [ ] Run existing regression tests

## Decisions
- Per-axis maxAngle replaces sensitivity entirely (no global multiplier)
- Display labels swapped (Pitch <-> Roll) for landscape; internal naming unchanged
- GL error: investigate and suppress only; full GLES 2.0 migration deferred
- Only BTN_GYRO_RECENTER buttons trigger recenter in editor
- Old configs migrate gracefully (old maxAngle -> all three per-axis values, old sensitivity ignored)
