# Plan: Test report fixes, RT fire primary, background pause

## Issue 1: RT fire primary still broken (user item #3)

### Root cause
In `buildJoyPairs()`, when processing "RT" -> "Fire Primary":
- `btnKcIdx = btnKcMap["Fire Primary"]` = 0 (non-null)
- `BUTTON_CONTROLS["RT"]` = 21 (non-null)
- The face-button skip: `if (btnKcIdx != null && BUTTON_CONTROLS[controlId] != null) continue`
- This skips RT before it reaches the axis-button handling code
- RT is in BUTTON_CONTROLS (SDL button 21) AND AXIS_CONTROLS (SDL axis 5)
- For face buttons (A/B/X/Y etc.), the skip is correct - identity + mixer handles them
- For RT/LT, the primary input comes via axis events, not key events
- The skip prevents the axis-button SDL index from being written to the config
- Result: C joy_axisbutton_handler generates button 21 but no kconfig entry matches

### Fix
- [x] Add `&& !AXIS_CONTROLS.containsKey(controlId)` to skip check in buildJoyPairs
- This lets RT/LT fall through to the axis-button code when bound to button funcs

## Issue 2: Background pause toggles game menu (user item #2)

### Root cause
`nativeOnPause()` in android_input.c unconditionally injects Escape key.
Escape toggles the game menu. If menu is already open -> Escape closes it.
User wants: menu always open (paused) after backgrounding.

### Fix
- [x] Check `window_get_front() != Game_wind` before injecting Escape
- If a menu window is already covering the game, skip the injection
- nativeIsInGame() already calls window_get_front() on the same thread, so safe

## Issue 3: Test report analysis (user item #1)

### Pre-existing failures (same in 03-26 and 03-27 reports)
- test_axis_mapping: heading_time stays 0 after send_axis -- possibly an issue with
  how send_axis interacts with the event-driven kconfig, or a test design issue.
  Phase 1 (static binding assertions) passes. Needs runtime debugging
- test_dpad_triggers: throttle_time stays 0 after send_axis for triggers. The default
  RT->Accelerate binding uses a virtual combiner axis (axis 8). send_axis sends
  raw axis 5. Controls.joy_axis[8] is never updated because the combiner runs in
  Kotlin, which send_axis bypasses. Need to either fix the test or the send_axis path
- test_resolution (ps1): default half-screen resolution not applied (640x480 vs expected)
- test_saf_archiver: descent2.ham not found at expected path (data layout issue)
- test_mp: TLS packet header parse error (networking/cert issue)
- test_extract: empty log (test infrastructure issue)
- test_autoselect_crash: timeout
- test_all_extracts: timeout
- test_lan: timeout

### Priority for this session
- Fix #1 RT fire primary: simple code fix
- Fix #2 background pause: simple code fix, both d1 and d2
- Analysis for test failures: document findings, defer runtime debugging

## Build
- [x] Build and verify - BUILD SUCCESSFUL
- [x] Run code quality - all pass (ktlint issue is pre-existing BuildInfo.kt)
