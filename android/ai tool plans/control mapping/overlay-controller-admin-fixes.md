# Plan: Automap Overlay, Controller, Admin Tray, and Related Fixes

## Phase 1: Automap Overlay Switching Lag (D1 + D2)

**Root cause:** Overlay polls automap state every 500ms (MainActivity.kt ~line 536).
Up to 500ms latency before overlay switches to automap mode or back.

**Fix:**
1. Reduce polling interval from 500ms to ~100ms in `startOverlayPolling()`
2. When MAP button is pressed (toggleAutomap): immediately set `automapActive`
   on the Kotlin side rather than waiting for the poll. Reverse on exit.
3. Call `invalidate()` after immediate state change to force redraw.

**Files:** MainActivity.kt, TouchOverlayView.kt

---

## Phase 2: D1 Automap Marker Buttons

**Root cause:** `NativePilotPatcher` loads `dxx-redux-d2` during setup. When
D1 runs, `nativeGetMarkerCount()` resolves from the D2 library. D2's
`MarkerObject[]` is zero-initialized (not -1), so all 10 entries count as
"placed"

**Fix:**
1. In MainActivity where `markerCountProvider` is set: if `game == "d1"`,
   hardcode provider to return 0 (D1 has no markers)
2. Refresh marker count when `automapActive` becomes true (call
   `recomputeAutomapBtnRects()`)
3. For D2: only show marker buttons when actual count > 0

**Files:** MainActivity.kt, TouchOverlayView.kt

---

## Phase 3: D1 Gyro Center Button + Axis Issues

**Center button:** Code path looks correct on paper. Add logging to confirm
the JNI call fires and automap.c consumes it.

**Gyro axes "seem wrong":** Build-919 had an axis swap fix for D2. D1 may
need the same. Check GyroInputManager.kt for D1/D2 conditional mapping.

**Files:** android_input.c, d1/main/automap.c, GyroInputManager.kt

---

## Phase 4: Touch Editor D2-Only Labels

**Issue:** Controller editor shows "(D2 only)" suffix grayed/italic in D1
mode. Touch editor hides D2-only controls entirely instead.

**Fix:** In ButtonBindingPicker (TouchEditorPage.kt), show D2-only entries
with "(D2 only)" suffix, grayed (0xFF999999), italic -- matching the
controller editor pattern. Make them non-selectable in D1 mode.

**Files:** TouchEditorPage.kt

---

## Phase 5: Old Pilot Files Cleanup

**Root cause:** `deleteAllPilotFiles()` scans sets/<name>/d1x-redux/ and
d2x-redux/ (plus Players/ subdirs). Old pilot files from before d1/d2
separation may be in filesDir/ root.

**Fix:** Add filesDir root and filesDir/Players/ to deleteAllPilotFiles scan.

**Files:** FileSetManager.kt

---

## Phase 6: Gamepad Connect Crash Hardening

**Issue:** Controller edit screen polls gamepads every 1s. Hotplug can race
with Compose recomposition.

**Fix:** Add try/catch around gamepads computation, null-safety guards
around InputDevice method calls.

**Files:** ControllerConfigPage.kt

---

## Phase 7: D2 Analog Trigger Slide Up/Down

**Issue:** LT/RT bound to Slide Down/Slide Up do nothing in D2.

**Context:** Controller editor green bars show correct 0-100% trigger values,
confirming Android -> Kotlin axis path works. Problem is downstream.

**Investigation focus (C side only):**
1. Add logging in joy_axisbutton_handler for axes 4,5 to confirm button
   events fire with correct button indices
2. Add logging in kconfig_read_controls for button events matching slide
   up/down kconfig entries
3. Check that kc_joystick[8/9].value is correctly set from PlayerCfg
4. Fix whatever gap is found

**Files:** d2/arch/sdl/joy.c, d2/main/kconfig.c, android_gamepad_config.cpp

---

## Phase 8: Admin Settings Quick Tray

**Design:** Visible small tab at bottom of screen (in-game only, not automap,
not menus). Tap or drag up to reveal a slide-up panel. Tap outside or swipe
down to dismiss.

**Items (6 total -- no "Abort level" since "Open game menu" covers it):**

| Item | Implementation |
|------|---------------|
| Increase view | nativeCycleCockpit(+1) -- forward through cockpit modes |
| Decrease view | nativeCycleCockpit(-1) -- backward through cockpit modes |
| Toggle auto-leveling | nativeToggleAutoLeveling() -- flip PlayerCfg |
| Quick save | Inject Alt+F2 key combo |
| Quick load | Inject Alt+F3 key combo |
| Open game menu | Inject ESC |

**New C functions needed:**
- `nativeCycleCockpit(int direction)` -- calls toggle_cockpit or reverse variant
- `nativeToggleAutoLeveling()` -- flips PlayerCfg.AutoLeveling + PF_LEVELLING
- `nativeGetAutoLeveling()` -- query for UI display
- `nativeGetCockpitMode()` -- query for UI display
- Add `toggle_cockpit_reverse()` to d1/main/gamerend.c and d2/main/gamerend.c

**Files:** TouchOverlayView.kt, MainActivity.kt, android_input.c,
d1/main/gamerend.c, d2/main/gamerend.c, d1/main/object.c, d2/main/object.c

---

## Phase 9: Default Controller Config (Single Source)

**Goal:** Eliminate FALLBACK_BINDINGS duplication. The default.json asset
is the single source of truth. Embedded in the APK, always loadable.

**Approach:**
1. Update default.json to match the user's controller_config.json
2. Remove FALLBACK_BINDINGS map from ControllerConfigPage.kt
3. Change loadDefaultBindings() error path: if asset loading fails, return
   an empty map (or throw) instead of silently falling back. Asset load
   from APK should never fail in practice.
4. Remove the C-side hardcoded defaults block in android_gamepad_config.cpp
   (the "No controller_config.json" branch). Instead, have the C code read
   the default.json from assets via `g_asset_manager` (existing infra in
   jni_main.c) when the on-disk config file is missing. Single source of
   truth = the bundled JSON asset.

**Files:** default.json, ControllerConfigPage.kt, android_gamepad_config.cpp

---

## Phase 10: Build, Lint, Test

1. Run `android/run-code-quality.ps1 --fix`
2. Build APK: `./gradlew clean assembleDebug`
3. Install and run tests on emulator
4. Desktop cmake build to verify no d1/d2 regressions from C changes
