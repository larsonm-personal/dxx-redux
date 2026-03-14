# Plan: Black Screen Fix Integration Test

## Goal
Add integration test steps to verify the EGL surface recreation fix works after
minimize/resume. Uses the existing automation and introspection infrastructure.

## Design

### New introspection field: `egl_recreate_count`
- Counter in gr.c incremented each time `ogl_android_recreate_egl_surface()` runs
- Exposed via `ogl_get_egl_recreate_count()` accessor
- Serialized in `game_introspect.cpp` as top-level `egl_recreate_count` field
- Value starts at 0; becomes 1 after one background/resume cycle

### Test flow
1. Existing test_launch_to_automap.json5 gets the game into a level
2. After automap verification, assert `egl_recreate_count` is 0 (no resume yet)
3. Log `SCRIPT_BACKGROUND: ready` marker
4. `wait_for` egl_recreate_count to become "1" (timeout 30s)
5. Test runner detects `SCRIPT_BACKGROUND:` in logcat, presses HOME via ADB,
   waits, then brings app back with `adb shell am start`
6. On resume, EGL surface is recreated, counter increments to 1
7. `wait_for` succeeds, script continues with post-resume assertions
8. Assert in_game=true, screen_mode=game, egl_recreate_count >= 1
9. PASS

### Test runner changes (Watch-AutomationResult)
When the logcat monitor detects `SCRIPT_BACKGROUND:`, it:
1. Waits 2 seconds (let game settle)
2. Sends `adb shell input keyevent KEYCODE_HOME`
3. Waits 5 seconds (app is in background)
4. Re-launches with `adb shell am start -n com.dxxredux.app/.MainActivity`
5. Waits 3 seconds (let app resume)
6. Continues monitoring for PASS/FAIL

### Files modified
- d1/arch/ogl/gr.c -- add g_egl_recreate_count + accessor
- d2/arch/ogl/gr.c -- same
- android/app/src/main/cpp/shared/game_introspect.cpp -- add field
- android/test_helpers.ps1 -- Watch-AutomationResult background handling
- android/game_scripts/test_launch_to_automap.json5 -- add test steps
