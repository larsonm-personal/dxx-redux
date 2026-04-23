# Outstanding bugs work plan

Source: [android/outstanding_bugs.md](android/outstanding_bugs.md). Each bug below has: scope, code anchors, suspected cause, planned change, and a verification/test plan. Bugs are roughly ordered easy-to-hard so the early ones can land independently. Mark items `[done]` as work completes.

---

## 1. [in progress] D1 (and possibly D2) load-game preview snapshots are corrupted

### Scope
- D1 save/load preview thumbnails draw with wrong colors on Android.

### Code anchors
- [d1/main/state.c](d1/main/state.c#L515-L545) `state_callback()` -- still uses `ogl_ubitmapm_cs(...)` for the thumbnail.
- [d2/main/state.c](d2/main/state.c#L549-L590) `state_callback()` -- already fixed to use `ogl_ubitblt_i()` so the indexed thumbnail uploads against `gr_current_pal`.
- Repo memory: `/memories/repo/d2-save-preview-ogl-palette-path.md`.

### Cause (updated after follow-up code study)
The earlier D1 draw-path change was only an experiment, not a sufficient fix. The stronger mismatch is in thumbnail serialization: D1 save files still store only indexed thumbnail bytes, but not the palette those bytes were quantized against. D2 already stores a 256x3 palette blob after the thumbnail and remaps on load. A renderer-only change cannot recover the intended colors for D1 thumbnails once that palette information was discarded.

### Planned change
- Add a small save-thumbnail decode path in C that reads thumbnail bytes from a save file and, when a palette blob is present, remaps them into the current runtime palette before display.
- Bump the D1 save version so newly written D1 saves also serialize the thumbnail palette after the thumbnail bytes, matching the D2 shape closely enough to share decode logic later.
- Keep compatibility with older D1 saves by falling back to the legacy raw thumbnail read when no palette blob exists.

### Current status
- Landed a save-thumbnail decode path in both `d1/main/state.c` and `d2/main/state.c` so previews are created from save-file thumbnail data plus an optional palette blob.
- D1 now writes the thumbnail palette in new saves (`STATE_VERSION 8`) and consumes it on preview load and full restore; D2 was refactored onto the same helper path and now always writes the palette blob even on the blank-thumbnail fallback.
- Older D1 saves remain compatible, but they still have only the legacy raw thumbnail bytes, so they can only be displayed on a best-effort basis.
- Passed `run-windows-build.ps1 -Target both` after the native changes and again after the code-quality pass.
- Still needs Android manual verification with a newly created D1 save, plus a D2 regression check.

- Build D1 Android, in the emulator: start any pilot, save to slot 1, exit to main menu, re-enter Load Game and verify the thumbnail colors match the in-game scene.
- Repeat in D2 to confirm no regression.
- Add an automation script `android/game_scripts/test_save_load_preview.json5` that drives: new game -> first level -> save -> load menu -> introspect to confirm menu opened. Visual correctness still needs by-hand check, but the script catches crashes/regressions in the flow.

---

## 2. [in progress] In-level pause and game menu (save/load/quit) have filtering applied even when graphics settings have menu filtering off

### Scope
- In-game pause/menu textures (background/UI sprites) ignore `GameCfg.MenuTexFilt`.

### Code anchors
- [d1/arch/ogl/ogl.c](d1/arch/ogl/ogl.c#L490-L520) and [d2/arch/ogl/ogl.c](d2/arch/ogl/ogl.c#L490-L520) -- the `MenuTexFilt`/`g_ogl_render_context` decision for font textures.
- `g_ogl_render_context` is toggled in [d1/main/gauges.c](d1/main/gauges.c#L4215) and [d2/main/gauges.c](d2/main/gauges.c#L4215). It is set to `0` only in cockpit/reticle paths.
- [d1/main/gamerend.c](d1/main/gamerend.c#L685) `show_boxed_message()` and surrounding pause/menu render paths.

### Cause (confirmed by code path study)
Pause and game-menu (save/load/quit launched from in-game) draw through menu/boxed-message handlers while `g_ogl_render_context` is still set to the gameplay value (`1`) from the preceding 3D/HUD frame. `gr_flip()` only resets the context to menu after the frame is already drawn, so the menu background and text textures bind with gameplay filtering instead of `MenuTexFilt`.

### Planned change
- Force `g_ogl_render_context = 0` only while `newmenu_draw()` / `listbox_draw()` execute, then restore the previous context immediately after the draw.
- Apply the same temporary context override around `show_boxed_message()` so the pause overlay uses `MenuTexFilt` too.
- Leave the broader OGL filter selection logic unchanged; the bug is the context at these draw entry points, not the texture-filter decision table itself.

### Current status
- Landed a narrower equivalent fix instead of introducing a new global flag: `d1/d2 main/newmenu.c` now force `g_ogl_render_context = 0` only while `newmenu_draw()` / `listbox_draw()` run, then restore the previous context.
- `d1/d2 main/gamerend.c` now do the same around `show_boxed_message()`, which covers the pause overlay path separately from newmenus.
- Passed `:app:externalNativeBuildDebug`, `run-windows-build.ps1 -Target both`, and `android\run-code-quality.ps1 -Fix`.
- Still needs manual in-game verification with `MenuTexFilt=0` and filtered world textures enabled.

### Test plan
- Set `MenuTexFilt=0` and `TexFilt=anisotropic` in graphics settings.
- Start a level, press Esc to open the in-game menu. Visually verify menu text and background use nearest-neighbour, not linear.
- Open Save Game from in-level and confirm thumbnail and menu text are also unfiltered.
- Add `android_texture_debug` introspection (already present) to dump min/mag filter for the active menu draw to a debug log entry, gated behind a debug category, so the regression can be caught by log scraping rather than screenshots.

---

## 3. [ignored] Multiple debug logs produced for a single app run

### Scope
- User reports this is now fixed. Leave out of the current tranche unless it regresses.

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/DebugLog.kt](android/app/src/main/java/com/dxxredux/app/DebugLog.kt#L188-L235) `openLog()` / `reuseActiveLog()`.
- [android/app/src/main/java/com/dxxredux/app/SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L512) writes `netlog_path` extra into the launch intent.
- [android/app/src/main/java/com/dxxredux/app/MainActivity.kt](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L420-L430) honors `netlog_path` extra.
- Repo memory: `/memories/repo/debug-log-session-handoff.md`.

### Suspected causes (need narrowing)
- a) Setup -> Main handoff loses the `netlog_path` extra on TV because the launcher path differs (e.g. monkey/recents resume vs `am start -n`), and the 15-minute reuse window is missing or too short for slow TV launches.
- b) `openLog()` is being called more than once per process (e.g. each Activity onCreate), and `reuseActiveLog()` fails because `currentBuildStamp()` reads `BuildInfo.GIT_COMMIT_COUNT` after a code path that was modified.
- c) Activity recreation on configuration change (TV boots can flip locale/density) calls `init()` again and the active-log file's `lastModified()` already exceeded the window because of a slow first frame.

### Planned change
- None in this tranche.

### Test plan
- None unless the regression reappears.

---

## 4. [in progress] Android TV: only D-pad navigates launcher menus, left analog stick should also work

### Scope
- Launcher Compose UI (SetupActivity pages).

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/SetupActivity.kt](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L1192-L1265) `onGenericMotionEvent` already reads AXIS_X/Y/Z/RZ but only stores them into `controllerAxes` for the controller config visualization, never synthesizes navigation key events.
- [android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt](android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt) for the focus border setup.

### Planned change
- In `SetupActivity.onGenericMotionEvent`, when the source is joystick and `controllerConfigActive` is false, debounce-convert AXIS_X/Y crossings of +/-0.5 into synthetic `KeyEvent.KEYCODE_DPAD_*` events delivered to the focused View via `dispatchKeyEvent`. Use a per-axis "armed" boolean so a held stick fires once until it returns inside +/-0.25 (deadband + hysteresis).
- Skip while the controller-config page is active to avoid double-handling.

### Current status
- Landed in `SetupActivity.dispatchGenericMotionEvent()`: left-stick X/Y now feed the existing synthesized DPAD navigation path with hysteresis and HAT-axis priority.
- Passed `./gradlew.bat :app:compileDebugKotlin`.
- Still needs on-device focus-navigation verification on TV.

### Test plan
- TV emulator with a virtual gamepad: focus a button, push left stick down, confirm focus moves; release stick, confirm no auto-repeat without movement; push and hold, confirm only one move per push.
- Phone: confirm no regression with attached USB gamepad.
- Add unit-style test by injecting `MotionEvent` instances via `Espresso`/`onView(..).perform(...)` if practical; otherwise document the manual steps in `android/tests/`.

---

## 5. [in progress] Controller-on-TV: B sticks and fires all missiles in one go (may be stale)

### Scope
- Default binding has B = secondary fire. Pressing B once observed to dump the entire missile pile.

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/MainActivity.kt](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L2095-L2105) `dispatchDpad`/button mixer entry points.
- `MainActivity.kt` line 2272 ("route D-pad/A/B to admin tray when open") and the rerouted IME path around 2190-2220 -- both deliver synthetic events for B.
- [android/app/src/main/cpp/jni_main.c](android/app/src/main/cpp/jni_main.c) joy button injection (search for `joy_button_state` / `inject_button`).
- Repo memory: `/memories/repo/android-tv-keyboard-final-routing.md` and `android-tv-dpad-center-and-input-menu.md`.

### Cause (updated after follow-up code study)
The repeat-edge tracker removed one failure mode, but the gameplay path still routes the same physical press twice: once through `InputMixer` as `MIXER_BTN_BASE + kc_index`, and again through raw `nativeJoystickButton(joyBtn, ...)`. Because the launcher also patches native joystick bindings into `.plr` files, button `B` can still hit gameplay both through the mixer path and through the raw joystick path. The raw joystick event is only needed when a non-game window is frontmost so A/B can act like Enter/Esc in title/menu handlers.

### Planned change
- Keep the edge tracker, but gate raw joystick button injection so it only runs while the game window is not frontmost.
- Leave the mixer path active during gameplay, because that is the intended button-to-action translation path for controller bindings.

### Current status
- Kept the existing edge tracker and landed a narrower routing fix in `MainActivity.kt`: raw joystick button injection now only runs while a non-game UI window is frontmost, so gameplay uses only the mixer path.
- Added `GamepadButtonRoutingPolicyTest` and revalidated the controller-selection tests after the formatting pass.
- Still needs on-device verification that TV `B` now fires exactly once in gameplay while still acting as back/cancel in title and menu flows.

### Test plan
- TV with virtual gamepad: in level, press and release B once. Inspect introspection JSON for `player.secondary_ammo[*]` to confirm only one missile fired.
- Automation script `android/game_scripts/test_b_button_no_stick.json5` that injects a single button event via the automation driver and checks ammo delta == 1 via introspection.

---

## 6. [in progress] TV shows generic controller name instead of bluetooth gamepad name

### Scope
- Controller config page on TV labels the detected device as "Virtual" or similar generic name instead of e.g. "8BitDo Pro 2".

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L1670-L1695) -- `gamepads.first().name` is shown but the list is unsorted.

### Suspected cause
On TV, `InputDevice.getDeviceIds()` returns multiple devices including the system's "virtual" remote receiver before the actual BT pad. `gamepads.first()` picks the wrong one. USB gamepads on phones tend to be the only matching device, so it works there.

### Planned change
- Filter out devices where `device.isVirtual == true` (API 29+) and devices where `device.vendorId == 0 && productId == 0`. Sort remaining by `(external first, productId != 0 first, name length desc)` and pick that as the displayed name.
- Surface all matched devices in the introspection JSON (`controller_introspect.json`) so this can be inspected without screenshots.

### Current status
- Landed the missing `SetupActivity.kt` header fix so both launcher controller-name surfaces now use `selectDisplayedController(...)` instead of the first enumerated device.
- Revalidated with Kotlin compile and the existing `ControllerDeviceSelectionTest` after the code-quality pass.
- Still needs TV/device verification that the visible launcher header now shows the Bluetooth pad rather than the generic virtual device.

### Test plan
- TV emulator with a virtual gamepad attached: confirm displayed name matches the configured pad and not "Virtual".
- Phone with a single USB gamepad: confirm name unchanged.
- Add a unit test for the filter+sort helper using a list of fake `DeviceInfo` records.

---

## 7. [in progress] Mod loading near full storage crashes with no crash dump

### Scope
- Two sub-items per the bug: (a) get kotlin/native crash dumps in this case, (b) check free space proactively.

### Code anchors
- Mod extraction and loader path in `android/app/src/main/java/com/dxxredux/app/` (mod manager / file extraction). Search `extract`, `unzip`, `mod_loading`.
- `StatFs` is not used anywhere yet (grep result). Need a helper `FreeSpace.getAvailableBytes(filesDir): Long`.
- xCrash integration -- repo memory `/memories/repo/android-xcrash-tombstone-layout.md`.

### Planned change
- Add a shared storage guard using `StatFs` plus URI-size probing, and call it before launcher-side writes into `filesDir/tmp`, `mods/`, and final game-data directories.
- Surface low-space failures as user-visible import errors instead of letting the write path crash mid-stream.
- Keep xCrash as the crash-handler path, but also write `last_extract_error.txt` on handled launcher import failures so post-mortem does not depend on a tombstone when the app does not actually crash.

### Current status
- Code study confirmed xCrash is already initialized from `DxxReduxApp.attachBaseContext()` for all processes; no extra SetupActivity-specific crash-handler work was needed.
- Landed `ImportStorageGuard.kt` and used it in `ModManager.importMod()`, `extractZipContents()`, `extract7zContents()`, `downloadFile()`, the generic SAF `importFile()` helper, demo install copies, and the `import_files` command path.
- Handled failures now record `last_extract_error.txt` via `ImportStorageGuard.recordFailure(...)` and return user-visible error strings where the UI already supports them.
- Passed `:app:compileDebugKotlin` and `android\run-code-quality.ps1 -Fix`.
- Still needs emulator/device verification with low free space to confirm the launcher now fails cleanly with a message instead of crashing.

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
Land 1, 2, 4 first (small, mostly local). Then 6 and 5 together (both controller-on-TV). 7 last because it touches new error-reporting infrastructure. Loading bar after 7.
