# Outstanding bugs work plan

Source: [android/outstanding_bugs.md](android/outstanding_bugs.md). Each bug below has: scope, code anchors, suspected cause, planned change, and a verification/test plan. Bugs are roughly ordered easy-to-hard so the early ones can land independently. Mark items `[done]` as work completes.

---

## 1. [ ] D1 (and possibly D2) load-game preview snapshots are corrupted

### Scope
- D1 save/load preview thumbnails draw with wrong colors on Android.

### Code anchors
- [d1/main/state.c](d1/main/state.c#L515-L545) `state_callback()` -- still uses `ogl_ubitmapm_cs(...)` for the thumbnail.
- [d2/main/state.c](d2/main/state.c#L549-L590) `state_callback()` -- already fixed to use `ogl_ubitblt_i()` so the indexed thumbnail uploads against `gr_current_pal`.
- Repo memory: `/memories/repo/d2-save-preview-ogl-palette-path.md`.

### Cause (high confidence)
D1 thumbnails are remapped into the current palette at load time, but the cached bitmap uploader path used by `ogl_ubitmapm_cs` does not honor the runtime `gr_current_pal`. Same root cause as the D2 bug, just not yet patched.

### Planned change
- Mirror the D2 fix in D1 `state_callback()`: replace the OGL branch with the same `ogl_ubitblt_i(...)` call. Keep behind `#ifdef OGL` like D2 does. No format change.
- Confirm D1 load/save callers also remap into the current palette before `state_callback` runs (search for `gr_remap_bitmap_good`/equivalent in `d1/main/state.c`); if D1 stores raw indexed thumbnails without remap, add the same remap step that D2 uses on read so the assumption holds. If D1 uses a different stored format, leave as-is and instead remap in-place before drawing.

### Test plan
- Build D1 Android, in the emulator: start any pilot, save to slot 1, exit to main menu, re-enter Load Game and verify the thumbnail colors match the in-game scene.
- Repeat in D2 to confirm no regression.
- Add an automation script `android/game_scripts/test_save_load_preview.json5` that drives: new game -> first level -> save -> load menu -> introspect to confirm menu opened. Visual correctness still needs by-hand check, but the script catches crashes/regressions in the flow.

---

## 2. [ ] In-level pause and game menu (save/load/quit) have filtering applied even when graphics settings have menu filtering off

### Scope
- In-game pause/menu textures (background/UI sprites) ignore `GameCfg.MenuTexFilt`.

### Code anchors
- [d1/arch/ogl/ogl.c](d1/arch/ogl/ogl.c#L490-L520) and [d2/arch/ogl/ogl.c](d2/arch/ogl/ogl.c#L490-L520) -- the `MenuTexFilt`/`g_ogl_render_context` decision for font textures.
- `g_ogl_render_context` is toggled in [d1/main/gauges.c](d1/main/gauges.c#L4215) and [d2/main/gauges.c](d2/main/gauges.c#L4215). It is set to `0` only in cockpit/reticle paths.
- [d1/main/gamerend.c](d1/main/gamerend.c#L685) `show_boxed_message()` and surrounding pause/menu render paths.

### Suspected cause
Pause and game-menu (save/load/quit launched from in-game) are reached *while the game window is still front* and `g_ogl_render_context` is left in the 3D-render value (1). The font-filter branch then takes the gameplay-context decision rather than the menu decision, so `MenuTexFilt=0` is ignored. The same context bit also drives non-font texture filter selection paths invoked while drawing the menu background.

### Planned change
- Add a small "menu mode active" boolean (e.g. `g_ogl_in_menu_overlay`) set true around `newmenu`/`listbox` window draws when the topmost window is a menu, and consult it in the filter selection branches alongside `g_ogl_render_context`.
- Set/clear in `newmenu_handler`/`listbox_handler` `EVENT_WINDOW_DRAW` entry/exit (both d1 and d2; this is one of the must-duplicate spots).
- Apply OGL state in `gr_flip()` per the per-frame note in the project guidelines, since menus do not call `ogl_start_frame`/`ogl_end_frame`.

### Test plan
- Set `MenuTexFilt=0` and `TexFilt=anisotropic` in graphics settings.
- Start a level, press Esc to open the in-game menu. Visually verify menu text and background use nearest-neighbour, not linear.
- Open Save Game from in-level and confirm thumbnail and menu text are also unfiltered.
- Add `android_texture_debug` introspection (already present) to dump min/mag filter for the active menu draw to a debug log entry, gated behind a debug category, so the regression can be caught by log scraping rather than screenshots.

---

## 3. [ ] Multiple debug logs produced for a single app run

### Scope
- A single app run produces several `debuglog_*.txt` files instead of one. Worse on TV but possibly all platforms. Regressed since 22 May.

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/DebugLog.kt](android/app/src/main/java/com/dxxredux/app/DebugLog.kt#L188-L235) `openLog()` / `reuseActiveLog()`.
- [android/app/src/main/java/com/dxxredux/app/SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L512) writes `netlog_path` extra into the launch intent.
- [android/app/src/main/java/com/dxxredux/app/MainActivity.kt](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L420-L430) honors `netlog_path` extra.
- Repo memory: `/memories/repo/debug-log-session-handoff.md`.

### Suspected causes (need narrowing)
- a) Setup -> Main handoff loses the `netlog_path` extra on TV because the launcher path differs (e.g. monkey/recents resume vs `am start -n`), and the 15-minute reuse window is missing or too short for slow TV launches.
- b) `openLog()` is being called more than once per process (e.g. each Activity onCreate), and `reuseActiveLog()` fails because `currentBuildStamp()` reads `BuildInfo.GIT_COMMIT_COUNT` after a code path that was modified.
- c) Activity recreation on configuration change (TV boots can flip locale/density) calls `init()` again and the active-log file's `lastModified()` already exceeded the window because of a slow first frame.

### Planned change (after diagnosis)
- Add one debug line in `openLog()` recording: callsite (Setup vs Main, whether `initAppend` vs `init`), whether `reuseActiveLog` returned true, and which file was opened. Push and run a typical TV+phone start sequence, then read the resulting log.
- Once the fault path is known: most likely the fix is to also check the prefs path before rotating in `initAppend()` (currently it always opens the supplied path even if it differs), and to make `rememberActiveLog` survive process restart by storing the path *before* the first write.
- Also widen the reuse window from 15 -> 30 minutes if cold-start logs show TV `lastModified` deltas approaching the limit.

### Test plan
- Phone: cold start app, play 1 level, exit. Confirm exactly one `debuglog_*.txt` for the run.
- TV: same.
- Long pause case: Setup screen -> wait 5 min -> launch game -> verify one file.
- Add a one-liner shell helper `android/check_log_count.ps1` that runs `adb shell run-as com.dxxredux.app ls -1 files/debuglogs | wc -l` before/after a scripted run.

---

## 4. [ ] Android TV: only D-pad navigates launcher menus, left analog stick should also work

### Scope
- Launcher Compose UI (SetupActivity pages).

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L1192-L1265) `onGenericMotionEvent` already reads AXIS_X/Y/Z/RZ but only stores them into `controllerAxes` for the controller config visualization, never synthesizes navigation key events.
- [android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt](android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt) for the focus border setup.

### Planned change
- In `SetupActivity.onGenericMotionEvent`, when the source is joystick and `controllerConfigActive` is false, debounce-convert AXIS_X/Y crossings of +/-0.5 into synthetic `KeyEvent.KEYCODE_DPAD_*` events delivered to the focused View via `dispatchKeyEvent`. Use a per-axis "armed" boolean so a held stick fires once until it returns inside +/-0.25 (deadband + hysteresis).
- Skip while the controller-config page is active to avoid double-handling.

### Test plan
- TV emulator with a virtual gamepad: focus a button, push left stick down, confirm focus moves; release stick, confirm no auto-repeat without movement; push and hold, confirm only one move per push.
- Phone: confirm no regression with attached USB gamepad.
- Add unit-style test by injecting `MotionEvent` instances via `Espresso`/`onView(..).perform(...)` if practical; otherwise document the manual steps in `android/tests/`.

---

## 5. [ ] Controller-on-TV: B sticks and fires all missiles in one go (may be stale)

### Scope
- Default binding has B = secondary fire. Pressing B once observed to dump the entire missile pile.

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/MainActivity.kt](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L2095-L2105) `dispatchDpad`/button mixer entry points.
- `MainActivity.kt` line 2272 ("route D-pad/A/B to admin tray when open") and the rerouted IME path around 2190-2220 -- both deliver synthetic events for B.
- [android/app/src/main/cpp/jni_main.c](android/app/src/main/cpp/jni_main.c) joy button injection (search for `joy_button_state` / `inject_button`).
- Repo memory: `/memories/repo/android-tv-keyboard-final-routing.md` and `android-tv-dpad-center-and-input-menu.md`.

### Suspected cause
The IME-reroute path and the gamepad-dispatch path both fire ACTION_DOWN for `BUTTON_B`, but only one path delivers the matching ACTION_UP. The button latches "down" in `joy_button_state[]` and the engine treats it as auto-repeat secondary fire until something clears it.

### Planned change (gated on reproduction)
- First, reproduce on the current build. The bug may already be gone after the recent IME flag/dispatch-depth work; if it does not reproduce in 3 attempts, mark as `[likely-fixed]` and close once a regression script exists.
- If it reproduces: add a `inject_joy_button(idx, down)` log line on every state edge (debug build only, DLOG_GAME) so the doubled-DOWN or missing-UP shows up in the debug log, then patch the offending path so each physical event yields exactly one DOWN/UP pair.

### Test plan
- TV with virtual gamepad: in level, press and release B once. Inspect introspection JSON for `player.secondary_ammo[*]` to confirm only one missile fired.
- Automation script `android/game_scripts/test_b_button_no_stick.json5` that injects a single button event via the automation driver and checks ammo delta == 1 via introspection.

---

## 6. [ ] TV shows generic controller name instead of bluetooth gamepad name

### Scope
- Controller config page on TV labels the detected device as "Virtual" or similar generic name instead of e.g. "8BitDo Pro 2".

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L1670-L1695) -- `gamepads.first().name` is shown but the list is unsorted.

### Suspected cause
On TV, `InputDevice.getDeviceIds()` returns multiple devices including the system's "virtual" remote receiver before the actual BT pad. `gamepads.first()` picks the wrong one. USB gamepads on phones tend to be the only matching device, so it works there.

### Planned change
- Filter out devices where `device.isVirtual == true` (API 29+) and devices where `device.vendorId == 0 && productId == 0`. Sort remaining by `(external first, productId != 0 first, name length desc)` and pick that as the displayed name.
- Surface all matched devices in the introspection JSON (`controller_introspect.json`) so this can be inspected without screenshots.

### Test plan
- TV emulator with a virtual gamepad attached: confirm displayed name matches the configured pad and not "Virtual".
- Phone with a single USB gamepad: confirm name unchanged.
- Add a unit test for the filter+sort helper using a list of fake `DeviceInfo` records.

---

## 7. [ ] Mod loading near full storage crashes with no crash dump

### Scope
- Two sub-items per the bug: (a) get kotlin/native crash dumps in this case, (b) check free space proactively.

### Code anchors
- Mod extraction and loader path in `android/app/src/main/java/com/dxxredux/app/` (mod manager / file extraction). Search `extract`, `unzip`, `mod_loading`.
- `StatFs` is not used anywhere yet (grep result). Need a helper `FreeSpace.getAvailableBytes(filesDir): Long`.
- xCrash integration -- repo memory `/memories/repo/android-xcrash-tombstone-layout.md`.

### Planned change
- Add a `FreeSpace` helper using `StatFs(context.filesDir.path)` and a `requireFreeSpace(needed)` that throws a typed exception with the deficit. Call it before every extraction-into-game-data loop with a conservative estimate (sum of compressed sizes * 2 + 50 MiB headroom).
- Show a launcher dialog ("Not enough space: needs N MiB free, have M MiB") instead of crashing.
- For the missing crash dump: confirm xCrash is initialized for SetupActivity (not just MainActivity) and that ANR/Java handlers are enabled. Add a wrapping try/catch around the extraction worker that logs to DLOG_GAME and writes a one-line file `last_extract_error.txt` so post-mortem doesn't require a tombstone.

### Test plan
- Fill emulator filesystem with `dd if=/dev/zero of=/data/local/tmp/filler bs=1M count=N` until `StatFs` reports < 100 MiB free.
- Attempt to load a 200 MiB mod. Verify dialog, no crash, log file present.
- Remove filler, verify normal mod load still works.

---

## Nice-to-have: "Prepare for Descent" loading progress bar

### Scope
- Long load with hires textures shows a static screen for up to 5 s.

### Code anchors
- [d1/main/gamerend.c](d1/main/gamerend.c#L685) and [d2/main/gamerend.c](d2/main/gamerend.c#L1061) `show_boxed_message()`.
- `paging.c` -> `show_boxed_message(TXT_LOADING, 0)` is called once before texture pageins.

### Planned change (later)
- Add an `android_loading_progress_set(int done, int total, const char *label)` helper and call it from the texture-pageing loop. Throttle to 1 update per 250 ms.
- In `show_boxed_message` (Android only), when progress is non-zero render a thin progress bar and the label below the message.

### Test plan
- Run a level with the largest hires texture pack on emulator, verify bar advances and last-loaded texture name updates.

---

## Cross-cutting: integration test scaffolding additions

After each fix above lands, extend or add to:
- `android/run_all_tests.ps1` to include the new `*.json5` scripts.
- `android/tests/` (kotlin unit tests) for the filter/sort helpers.

## Sequencing recommendation
Land 1, 2, 3, 4 first (small, mostly local). Then 6 and 5 together (both controller-on-TV). 7 last because it touches new error-reporting infrastructure. Loading bar after 7.
