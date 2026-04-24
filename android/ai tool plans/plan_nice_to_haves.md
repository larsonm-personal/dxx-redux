# Nice-to-haves plan

Source: `android/outstanding_bugs.md` "nice to haves" section. Controls-editor long-press is the first thing to implement and is fully specced. The rest are surveyed with file/function anchors so a later tranche can pick them up without re-exploring.

Status legend: `[ ]` not started, `[~]` in progress, `[x]` done.

---

## 1. Controls editor: long-press to open binding editor  [x]

User requirement (verbatim):
> controls editor needs to have a route to open the edit axis/button binding page for a specific axis/button if it's held for 2 seconds. that would enable quick controller-only bindings setup. for axes, the detection would be looking for an axis held to >80% of full range in a single direction without any other axis going >30% or any other button being pressed (for 2 seconds). for buttons, a button held >2s with no axis >30% or other button presses. only run this detection on the main controller edit page, don't open it for other bindings once a particular binding sub menu is shown.

### Implemented result

- Added `ControllerLongPressDetector.kt` as a pure Kotlin detector with unit tests.
- Integrated it into `ControllerConfigPage.kt` with a 50ms poll loop so held inputs still trigger on controllers that do not emit repeating motion events while stationary.
- Reused the existing tap routing through a shared `openControlPicker(...)` helper so long-press and tap open the same picker paths.
- Normalized button-name variants (`L2`/`R2`, `D-Up` etc.) back to the page's existing control ids.
- Extended long-hold detection to hat-axis D-pad input so D-pad holds can still open their assignment picker while short taps are used for page navigation.
- Added page-local controller navigation for the four main action buttons (Cancel, Save, Export, Import), with green selection highlighting and short-press `A` activation.
- Suppressed the short-press `A` action when the same hold was consumed by opening the `A` binding submenu.
- Added a fresh-sample fallback after closing stick/trigger/D-pad pickers: if no new axis sample arrives shortly after dismiss, the stale displayed value is zeroed until a real update arrives.
- Exposed stick-axis dead-zone sliders in non-button mode, with 10% defaults for `LS_*` / `RS_*` and the existing 30% threshold default retained for trigger/button-mode activation.

### Existing code anchors

- Main page composable: [android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L866) `fun ControllerConfigPage(...)`.
- Live controller state already piped into the composable:
  - [ControllerConfigPage.kt#L866-L888](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L866-L888) params `axes: FloatArray`, `dpadAxes: FloatArray`, `axisGeneration: Int`, `pressedButtons: SnapshotStateList<String>`.
- Activity plumbing providing those values:
  - [SetupActivity.kt#L1146-L1170](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L1146-L1170) fields `controllerAxes`, `dpadAxes`, `axisGeneration`, `pressedButtons`, `controllerConfigActive`.
  - [SetupActivity.kt#L1216-L1224](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L1216-L1224) fills those on every generic motion event.
  - [SetupActivity.kt#L1278-L1290](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L1278-L1290) fills `pressedButtons` on key events.
  - [SetupActivity.kt#L3287-L3295](android/app/src/main/java/com/dxxredux/app/SetupActivity.kt#L3287-L3295) gates `controllerConfigActive` via `DisposableEffect` around the page.
- Dialog state we need to gate against (open means sub-page is showing):
  - [ControllerConfigPage.kt#L931-L933](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L931-L933) `showButtonPicker`, `showStickPicker`, `showDpadPicker`.
- Dialog trigger path we will reuse to open a binding:
  - [ControllerConfigPage.kt#L985-L1000](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L985-L1000) sets `selectedControl = resolvedId` then flips one of the three `show*Picker` booleans based on the control family.
- Control id maps needed for reverse lookup (physical input -> control id):
  - `AXIS_CONTROLS` (control id -> SDL axis) [ControllerConfigPage.kt#L94-L102](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L94-L102).
  - `BUTTON_CONTROLS` (control id list) referenced at [ControllerConfigPage.kt#L943-L958](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L943-L958); defined earlier in the same file.
  - `DPAD_CONTROLS` used at [ControllerConfigPage.kt#L996](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L996).

### Design

Create a new `ControllerLongPressDetector` (plain Kotlin class, no Compose) in `android/app/src/main/java/com/dxxredux/app/ControllerLongPressDetector.kt`. Pure logic so it is unit-testable.

Fields:
- `longPressMs: Long = 2000L`
- `axisSelectThreshold: Float = 0.80f`
- `axisOtherThreshold: Float = 0.30f`

Public API:
- `fun update(nowMs: Long, axes: FloatArray, pressedButtons: List<String>, gated: Boolean): Trigger?`
  - Returns a `Trigger` when a long-press completes on this tick, else null.
  - `Trigger` is a sealed class: `Trigger.Axis(controlId: String, positive: Boolean)` or `Trigger.Button(controlId: String)`.
  - `gated == true` resets all timers and returns null. Called whenever any `show*Picker` is true, or the page is not front.

Internal state:
- `axisStartMs: LongArray(6)` indexed by the live analog axes array (0..5 for sticks/triggers). -1 when inactive.
- `axisSign: IntArray(6)` records the direction that started the press (-1 or +1).
- `buttonStartMs: MutableMap<String, Long>`.

Algorithm each `update`:
1. If `gated`, clear everything and return null.
2. Build "other activity" flag: any other axis (including hat) with `abs(value) >= axisOtherThreshold`, or any other button pressed.
3. For each axis index `i` in 0..5 plus hat (mapped to indices 6/7):
   - value = axes or dpadAxes slot; sign = value sign; mag = abs(value).
   - Fire condition: `mag >= axisSelectThreshold` and only this axis is above threshold and no button is pressed.
   - If fire condition and `axisStartMs[i] < 0` and same sign: set start = now, sign = sign.
   - If fire condition still holds and sign unchanged and now - start >= longPressMs: clear all state, return `Trigger.Axis(controlIdFor(i), positive = sign > 0)`.
   - Else if condition broken or sign flipped: clear this axis's state.
4. For each button in `pressedButtons`:
   - Fire condition: exactly one button pressed and all axes `< axisOtherThreshold`.
   - If fire and no start: record now.
   - If elapsed >= longPressMs: clear all state, return `Trigger.Button(controlId = button)`.
5. For any button no longer in `pressedButtons`: drop its timer.

Mapping helpers:
- `heldAxisControlId(axisIndex)` collapses LS/RS axis holds back to `LS` / `RS` and maps trigger axes to `LT` / `RT`.
- `heldButtonControlId(buttonName)` maps launcher key names back to the page's existing control ids.

### Integration in `ControllerConfigPage`

Add alongside existing state (near [ControllerConfigPage.kt#L931](android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt#L931)):

```kotlin
val longPressDetector = remember { ControllerLongPressDetector() }
LaunchedEffect(Unit) {
  while (true) {
    val trigger = longPressDetector.update(
      nowMs = SystemClock.elapsedRealtime(),
      axes = axes,
      pressedButtons = pressedButtons.toList(),
      gated = showButtonPicker || showStickPicker || showDpadPicker,
    )
    when (trigger) {
      is ControllerLongPressDetector.Trigger.Axis -> heldAxisControlId(trigger.axisIndex)
      is ControllerLongPressDetector.Trigger.Button -> heldButtonControlId(trigger.buttonName)
      null -> null
    }?.let(::openControlPicker)
    delay(50)
  }
}
```

The implementation uses a short poll loop instead of relying on repeated motion events from the controller.

### Cancellation

- Dialog open (`show*Picker == true`) sets `gated`, which clears timers.
- Navigating away unmounts the composable, which discards the `remember`-ed detector.
- Any other-input activity during the window clears that timer.

### Tests

Add `android/app/src/test/java/com/dxxredux/app/ControllerLongPressDetectorTest.kt`:
- axis held alone >= 2s -> returns `Axis` trigger once, then nothing on next tick.
- axis held but sign flips mid-window -> no trigger.
- axis held alongside a second axis >30% -> no trigger.
- axis held while a button is down -> no trigger.
- button held alone >= 2s -> returns `Button` trigger once.
- button held plus axis >30% -> no trigger.
- `gated = true` clears state and suppresses trigger even if otherwise qualifying.

### Validation

- Passed: `android/gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.ControllerLongPressDetectorTest"`
- Passed after formatting: `android/gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.ControllerLongPressDetectorTest"`
- Passed: `android/run-code-quality.ps1 -Fix`
- Passed: `run-windows-build.ps1 -Target both`
- Optional by-hand: emulator with a BT controller, hold LS up on the controller editor, confirm the stick picker opens after 2s. Hold A, confirm the button picker opens.

### Out of scope

- No changes in `kconfig.c` or `kconfig.h`; we only reuse the existing dialog flow.
- No change to the selected-control highlighting behavior on the canvas.

---

## 2. "Prepare for Descent" loading progress bar  [~]

User requirement:
> prepare for descent loading screen takes potentially up to ~5 seconds with high res textures. it would be nice to show a texture loading progress bar and maybe the text name of the most recently loaded texture or major step, or one text/bar update every 300ms max to limit drawing overhead.
>
> C code emits progress updates every 300ms. The launcher/Kotlin side displays them. Percent is computed as (items_done / total_items) with no weighting for individual texture size. Every 300ms the current texture name is sent along with the percent. The overlay is a half-transparent bar about 20% from the bottom of the screen, with a border and a filled progress region, and the current filename rendered as a less-transparent text overlay centered on the bar.

### Root cause / where the time goes

- `StartNewLevelSub` -> `LoadLevel` -> `piggy_load_level_data()` (fast) then `ogl_cache_level_textures()` (slow).
- `ogl_cache_level_textures()` in [d2/arch/ogl/ogl.c#L719](d2/arch/ogl/ogl.c#L719) and [d1/arch/ogl/ogl.c#L716](d1/arch/ogl/ogl.c#L716) loops `for (i = 0; i < Num_bitmap_files; i++) ogl_loadbmtexture(&GameBitmaps[i]);`. This is the main wall-clock cost on Android with hires textures. The Android branch already measures it into `g_cache_time_ms`.
- `paging_touch_all()` in [d2/main/paging.c#L340](d2/main/paging.c#L340) also walks bitmaps but pages them in from disk, not GPU upload. Fast enough that we do not need per-item progress for it, but it is the natural "phase 1" of the bar.
- The current UI during this time is just `show_boxed_message(TXT_LOADING, 0)` from [d2/main/gameseq.c#L776](d2/main/gameseq.c#L776). No refresh during the cache loop.

### Anchors (updated)

- Slow loop to instrument (primary): [d2/arch/ogl/ogl.c#L719](d2/arch/ogl/ogl.c#L719) and [d1/arch/ogl/ogl.c#L716](d1/arch/ogl/ogl.c#L716). Denominator = `Num_bitmap_files`. Per-item label = `piggy_game_bitmap_name(bm)` ([d2/main/piggy.h#L72](d2/main/piggy.h#L72)).
- Optional phase-0 loop: `piggy_load_level_data()` -> `paging_touch_all()` ([d2/main/paging.c#L340](d2/main/paging.c#L340), [d1/main/paging.c](d1/main/paging.c)). We surface it as a single coarse "Paging in level data..." label, not per-item.
- JNI overlay pattern to follow: [android/app/src/main/cpp/shared/android_jni_overlay.c](android/app/src/main/cpp/shared/android_jni_overlay.c) and [android_jni_overlay.h](android/app/src/main/cpp/shared/android_jni_overlay.h). Same `g_jvm` / `g_activity` globals, same `AttachCurrentThread` pattern.
- Throttle helper: Android `CLOCK_MONOTONIC` via `clock_gettime` (same pattern already used in `ogl_cache_level_textures`).

### Design

#### 1. Shared C helper (new)

Create under `android/app/src/main/cpp/shared/` so both d1 and d2 link it through their existing shared-sources globs:

- `android_loading_progress.h`
- `android_loading_progress.c`

Public API (header-guarded so non-Android host builds get empty stubs):

```c
/* Called once before a batch. total_items==0 means "indeterminate bar". */
void android_loading_progress_begin(const char *phase_label, int total_items);

/* Increment done count, remember most recent item label, flush to Kotlin at
 * most once every 300ms. Safe to call from any thread. label may be NULL. */
void android_loading_progress_step(const char *item_label);

/* Force one final flush at 100% and clear state. Kotlin hides overlay when
 * percent >= 100 or after a short grace period. */
void android_loading_progress_end(void);
```

Internals (Android build only):
- Static state: `int total`, `int done`, `char last_label[64]`, `struct timespec last_flush`, `int begun`.
- `android_loading_progress_step` always increments `done` and copies `item_label` (truncate to 63 chars, strip path), then checks the 300ms throttle using `CLOCK_MONOTONIC`. On throttle miss, return without a JNI call. On throttle hit, call one `flush()` helper.
- `flush()` computes `pct = total > 0 ? (100 * done + total/2) / total : 0`, attaches to `g_jvm`, and calls `MainActivity.showLoadingProgress(String phase, String item, int pct)` on the current activity instance.
- Thread safety: single producer in practice (GL thread during level load). Add a simple `pthread_mutex_t` guard since a second call from the audio thread would be harmless but race-free is cheap.
- All global state is trivial and lives in the file; no allocations.

Host-build stubs (used when `ANDROID` is not defined): empty functions in the same file under `#else`. This mirrors `android_jni_overlay.c`.

#### 2. Kotlin overlay (new)

Create `android/app/src/main/java/com/dxxredux/app/LoadingProgressOverlayView.kt`:

- A `View` (not SurfaceView) with its own `android:background="@android:color/transparent"`, added to `MainActivity`'s root `FrameLayout` above the GLSurfaceView and above `TouchOverlayView`. Initial visibility `GONE`.
- JNI entry point: companion-object `@JvmStatic fun onProgress(pct: Int, phase: String, item: String)`. Posts to the main thread `Handler` that updates internal state and calls `invalidate()`.
- `onDraw(canvas)`:
  - Bar rect: width = 0.8 * view.width, centered horizontally; height = `max(24.dp, 0.04 * view.height)`; top = `0.80 * view.height` (so the bar sits ~20% from the bottom as requested).
  - Background fill: paint alpha 128, color black (half-transparent).
  - Border: paint alpha 220, color white (or brand green `#3FBF3F` to match launcher accents), stroke width 2dp.
  - Progress fill: for `pct >= 0`, fill the inner rect from left to `left + pct/100 * innerWidth` with alpha ~192. For `pct < 0`, draw an indeterminate marquee (optional; first cut can skip and just show phase text).
  - Text: phase label above the bar (smaller), item label centered on the bar with alpha ~230 and a 1-pixel dark shadow for legibility. Font is Android default; no need to match the in-game font since this is a pure Android overlay.
- Hide logic: after `onProgress(100, ...)` the view stays visible for 250ms then sets itself `GONE`, so the jump from cache-end to first rendered frame is not jarring.
- No touch handling: `isClickable = false`, `isFocusable = false`; pass touches through.

#### 3. JNI wiring

- Reuse the existing `g_activity` instance-method pattern from `android_jni_overlay.c` instead of introducing a new static bridge.
- `android_loading_progress.c` calls `MainActivity.showLoadingProgress(String phase, String item, int pct)` during throttled updates and `MainActivity.hideLoadingProgress()` at the end of the batch.
- The activity methods hop back to the UI thread with `runOnUiThread { ... }`, keeping the native helper safe to call from the GL thread during texture uploads.

#### 4. Instrumentation call sites

D2:
- `d2/arch/ogl/ogl.c` `ogl_cache_level_textures()`:
  - Before the `for (i=0; i < Num_bitmap_files; i++)` loop: `android_loading_progress_begin("Caching textures", Num_bitmap_files);`
  - Inside the loop, after `ogl_loadbmtexture(bm)`: `android_loading_progress_step(piggy_game_bitmap_name(bm));`
  - After the loop (after `xmodel_load_gl_all()`): `android_loading_progress_end();`
- Optionally `d2/main/gameseq.c` `LoadLevel()` just before `piggy_load_level_data()`: `android_loading_progress_begin("Paging level data", 0);` then `android_loading_progress_end();` right after (gives the bar a "phase 0" heartbeat so it appears as soon as the loading screen does). Keep this only if the paging step is visible; skip if it adds visual noise.
- Mirror the exact same edits in D1 (`d1/arch/ogl/ogl.c`, `d1/main/gameseq.c`). Every edit guarded by `#ifdef ANDROID` to keep non-Android builds byte-identical.

Keep the d1/d2 diff minimal: use the same symbol names, same include line (`#include "android_loading_progress.h"` under `#ifdef ANDROID`), same 3 call sites.

#### 5. CMake / build

- Add `shared/android_loading_progress.c` to the shared sources glob in `android/app/src/main/cpp/CMakeLists.txt` (same list that includes `android_jni_overlay.c`). No new include dirs.
- For Windows/Linux/Mac host builds, the call sites are inside `#ifdef ANDROID`, so the helper header/source are not referenced at all. No build system changes required for host builds.

#### 6. Throttling correctness

- The throttle is in C (not Kotlin) to avoid the cost of repeated JNI attaches. A no-op `step()` reduces to an increment, a `clock_gettime` call, and a compare.
- `_end()` always flushes 100% once, bypassing the 300ms gate.
- First call after `_begin()` also bypasses the gate so the overlay appears immediately rather than 300ms into the load.

### Risks / edge cases

- `piggy_game_bitmap_name(bm)` can return an empty string for synthesized bitmaps; pass an empty `item` in that case and let Kotlin render just the phase text.
- `ogl_cache_level_textures()` runs on the GL thread; attaching to `g_jvm` from that thread is standard. We already do it in track-name overlays.
- If the helper is ever called before `g_jvm` is set (unlikely; set in `JNI_OnLoad` long before gameplay), `flush()` silently no-ops.
- Do not call `show_boxed_message()` from the progress helper. The existing boxed message already draws "Prepare for Descent" via the engine; the Kotlin overlay sits on top. This avoids reordering OGL state mid-upload.

### Implementation tranches

1. Shared C helper with host stubs + CMake entry + throttle. Compile-only on Windows.
2. Kotlin `LoadingProgressOverlayView` + `MainActivity` attach/detach.
3. JNI bridge: reuse the existing activity-instance callback path (`GetMethodID` + `CallVoidMethod` on `g_activity`).
4. Instrumentation in `ogl_cache_level_textures` for both d1 and d2, plus optional phase-0 call in `LoadLevel`.
5. Tests: focused JVM unit test for `LoadingProgressOverlayView` geometry and progress clamping. Host-side throttle test remains optional follow-up if the native helper needs more churn.
6. By-hand emulator validation on a large D2 level after a fresh install (cold cache, worst case).

### Validation

- Completed: `./gradlew.bat :app:testDebugUnitTest --tests LoadingProgressOverlayLayoutTest :app:assembleDebug` green.
- Completed: `android\run-code-quality.ps1 -Fix` green after a clean `android\stop-stale-formatters.ps1` check.
- Completed: `run-windows-build.ps1 -Target both -Compiler vs2022-community` green.
- Remaining by-hand check: on emulator or device, watch a fresh install load a big level and confirm the bar appears at ~20% from the bottom, grows smoothly, shows the last texture filename, updates no more than ~4 times per second, and disappears shortly after the game starts rendering.

### Implemented so far

- Added shared Android native helper `android_loading_progress.{c,h}` and wired it into both D1 and D2 `ogl_cache_level_textures()` loops.
- Added `LoadingProgressOverlayView` and `MainActivity` JNI entry points `showLoadingProgress()` / `hideLoadingProgress()` so the native helper can drive a top-layer overlay without touching the engine's boxed-message renderer.
- Added a focused JVM unit test for the overlay layout and progress clamping.

---

## 3. In-game Select button should open settings overlay, not save/load menu  [~]

User requirement:
> need some fixes for android TV to transition all remaining touch overlay bits to controller interfaces. the immediate need is for the in-game settings menu (the overlay one) to be the thing opened by the select button, rather than the in-game save/load/quit menu (its current function). there was a task at one point to move extra items into the settings menu when there was no touch interface: that needs to be rechecked.

### Anchors

- Current Select mapping: [MainActivity.kt#L2285-L2300](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L2285-L2300). Select currently calls `openGameMenuSafely()` (the save/load/quit menu) and Start toggles the admin tray.
- `openGameMenuSafely()`: [MainActivity.kt#L1333](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L1333).
- Admin tray (overlay settings): in `TouchOverlayView.kt` around the `adminTray*` block starting at [TouchOverlayView.kt#L659-L683](android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt#L659-L683). `toggleAdminTray()` is called from the Start handler and dispatches through `adminTrayCallback` at [MainActivity.kt#L646-L656](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L646-L656).
- Earlier task that moved items into the tray when touch is disabled: search for "gamepadOnlyMode" in `TouchOverlayView.kt` admin-tray action list, and the action enum at [MainActivity.kt#L2115-L2116](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L2115-L2116).

### Proposed approach

### Root cause summary

- `MainActivity.onKeyDown()` still has the old gamepad-only mapping: Start toggles the admin tray and Select opens the save/load/quit menu. That is the inverse of the current TV/controller requirement.
- The first controller-routing tranche was still too narrow because it keyed the Select/Start swap only off `gamepadOnlyMode` (`!FEATURE_TOUCHSCREEN`). On touchscreen devices with a Bluetooth controller, Select still fell through to the engine's regular game-menu path.
- The admin tray item list is still hard-coded as `9` base items or `14` gamepad-only items. That made it easy to add one fixed batch of controller-only items earlier, but it does not support layout-dependent items such as "show Automap only when the current touch layout does not already provide a touch Automap button".
- The earlier migration work is partly in place already. In `gamepadOnlyMode`, the tray already appends `Automap`, `Headlight`, `Warp`, `Music`, and `Accept`, and `MainActivity` already suppresses the standalone accept/warp overlays in that mode because those actions were moved into the tray.

### Research notes

- Already migrated into admin tray for gamepad-only use:
  - `Automap` via `TouchOverlayView.ADMIN_AUTOMAP` and `MainActivity` TAB injection handler.
  - `Headlight` via `TouchOverlayView.ADMIN_HEADLIGHT` and `MainActivity` H-key injection handler.
  - `Warp` via `TouchOverlayView.ADMIN_WARP` and the coop warp JNI helpers.
  - `Music` via `TouchOverlayView.ADMIN_MUSIC` and `showMusicPanel()`.
  - `Accept` via `TouchOverlayView.ADMIN_ACCEPT_JOIN` and `nativeAcceptJoinRequest()`.
- Not yet layout-aware:
  - `Automap` is appended unconditionally in `gamepadOnlyMode`, but touch-mode trays never gain it when a custom layout omits the touch Automap button.
- Already tray-backed outside the gamepad-only extras:
  - `Exit` is already a tray item and the standalone exit button is hidden when the touch overlay is active.

### Concrete steps

- [x] Recheck the current Start/Select routing and confirm the old mapping is still present in `MainActivity.onKeyDown()`.
- [x] Recheck the tray action inventory and confirm which touch-driven actions were already moved into controller-accessible grid items.
- [x] Replace the fixed admin-tray item count with a visible-action list helper so layout-dependent items can be added without hard-coded slot math.
- [x] Make `Automap` visible in the settings tray whenever the device is gamepad-only or the active touch layout does not include a button bound to `TouchBindings.BTN_AUTOMAP`.
- [x] Swap the gamepad-only Select/Start routing so Select opens the settings tray and Start opens the game menu, handling the case where the tray is already open.
- [x] Extend focused admin-tray unit coverage for the new visible-action-list rules.
- [~] Validate with Android compile/tests and a Windows host build.

### First implementation tranche

- Keep the first code change local to `MainActivity.kt`, `TouchOverlayView.kt`, and `AdminTrayUiTest.kt`.
- Treat `Automap` as the first layout-aware tray item because the user called it out explicitly and the supporting callback already exists.
- Preserve the existing tray labels, action ids, and callback handlers so the refactor only changes visibility/order, not behavior of existing actions.
- For controller routing, prefer explicit `open` / `close` calls over a blind toggle so Select cannot accidentally close the tray and Start can open the game menu without leaving the tray visible.

### Validation target for this tranche

- Android TV / gamepad-only: Select opens the settings tray; Start opens the save/load/quit menu.
- Touch layout without an Automap button: the settings tray includes an `Automap` item.
- Touch layout with an Automap button: touch-mode tray does not duplicate that action.
- Regression: existing tray actions still render and dispatch in their expected slots/order.

### Implemented result so far

- `MainActivity.kt` now routes gamepad-only Select to `openAdminTray()` and Start to `openGameMenuSafely()`, with an explicit tray close before opening the game menu if the tray was already visible.
- Controller Select/Start routing now keys off "settings tray is available here" rather than only `gamepadOnlyMode`, so touchscreen devices with an active touch overlay also open the settings tray on Select instead of falling through to the engine game menu.
- `TouchOverlayView.kt` now derives its admin tray from a visible-action list helper instead of assuming a fixed `9` or `14` items, so action order stays centralized while visibility can depend on runtime layout state.
- `Automap` is now present in the tray whenever `gamepadOnlyMode` is active or the current touch layout lacks any button bound to `TouchBindings.BTN_AUTOMAP`.
- Existing controller-migrated actions remain in place for gamepad-only mode: `Headlight`, `Warp`, `Music`, and `Accept` still append after the shared tray items.
- `AdminTrayUiTest.kt` now covers the touch-mode and gamepad-only visible-action rules for the admin tray.
- `OverlayVisibilityPolicyTest.kt` now covers the widened controller shortcut policy so touchscreen devices with an active gameplay overlay keep the Select-to-settings behavior.

### Validation so far

- Passed: `android\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.AdminTrayUiTest"`
- Remaining for this tranche: formatter pass, post-format Android compile/test rerun, and Windows host build

### Validation

- Android TV BT controller: press Select in-game, admin tray appears; press Start, save/load menu appears.
- Regression: existing phone+touch flows unchanged because this branch is under `if (gamepadOnlyMode)`.

---

## 4. Launcher slider focus highlighting  [ ]

User requirement:
> more launcher menus need highlighting. the sliders need green outlines when selected, for example.

### Anchors

- Pages containing sliders: `GraphicsPage.kt`, `EnginePage.kt`, anything in `android/app/src/main/java/com/dxxredux/app/` with `Slider(`.
- Existing dpad focus helper: check `DpadFocusUtils.kt` (exact filename to be confirmed by grep for `focusHighlight` or `onFocusChanged`). The existing buttons/rows use a green-tint modifier; sliders do not.

### Proposed approach

- Add `Modifier.sliderFocusOutline(focused: Boolean)` in the focus-utils file that draws a green 2dp border when focused.
- Wrap every launcher `Slider(...)` with a `remember { FocusRequester() }` + `onFocusChanged { focused = it.isFocused }` + the new modifier.
- Consider a higher-level `FocusableSlider(...)` wrapper to avoid duplicating boilerplate per page.

### Validation

- Android TV: navigate with dpad across the graphics page; every slider shows green outline when focused.

---

## 5. Buttons inside drag zones with slide-to-release  [~]

User requirement:
> allow buttons to be placed within the drag zones (they currently are allowed, but don't have special handling). if the button is pressed it stays active as long as the drag continues, then the button is released on drag stop. this will give another way to fire weapons while looking with the same thumb.

### Anchors

- Drag zone state: `TouchOverlayView.kt` mouse-mode / drag members near [TouchOverlayView.kt#L212-L240](android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt#L212-L240) and `beginMouseDrag` at [TouchOverlayView.kt#L444-L460](android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt#L444-L460).
- Buttons state list: the same file's `buttons` structure (search `b.pointerId` around [TouchOverlayView.kt#L994-L1000](android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt#L994-L1000)).
- Touch dispatch: `onTouchEvent` / `dispatchTouchEvent` in `TouchOverlayView.kt`.

### Proposed approach

### Root cause summary

- `TouchOverlayView.onTouchEvent()` currently hit-tests axis regions and sticks before layout buttons. A touch that lands inside a mouse-mode or floating-stick drag zone is claimed by the stick path first, so the later button hit-test never runs even if the finger is also on top of a button.
- Drag start still keys off the raw drag-zone rectangle, so a button that is visually placed inside the zone does not extend the valid drag-start area to the full button radius. The outer part of the button cannot start the drag unless the finger also happens to land inside the zone rect.
- Pointer teardown for `ACTION_POINTER_UP` assumes one pointer belongs to one control and stops after the first matching control. A shared drag+button pointer needs both the drag owner and the latched button owner to release on the same finger-up event.

### Concrete steps

- [x] Confirm the current ownership order in `TouchOverlayView.onTouchEvent()` and verify that overlapping drag-zone touches never reach the button hit-test.
- [x] Add a shared-pointer path for mouse-mode and floating-stick drag zones: when a touch starts inside the drag zone and also hits an eligible non-toggle button, start the drag and press the button with the same `pointerId`.
- [x] Extend drag-start hit testing so a button centered inside a drag zone contributes its full touch radius as a valid drag start area.
- [x] Keep the button latched while the drag continues. Reuse the existing button press/long-press code path instead of adding a separate drag-only button state.
- [x] Update `ACTION_POINTER_UP` / cancel teardown so a shared pointer releases both the drag owner and any latched button owner.
- [~] Validate with Android build/test passes and a by-hand touch layout check using a fire button placed inside a look drag zone.

### First implementation tranche

- Scope the shared-pointer behavior to stick drag zones (`mouseMode` and floating sticks), which is the user-visible “drag zone” feature surface in the current editor.
- Limit slide-to-release latching to non-toggle buttons so persistent toggles and one-shot admin-style buttons do not gain surprising drag semantics.
- A button only extends the drag-start area when its center point lies inside the drag zone; edge overlap alone does not expand the zone.
- Preserve existing behavior when there is no overlapping button or when the button is already owned by another pointer.

### Implemented result

- `TouchOverlayView.kt` now allows mouse-mode and floating-stick drag zones to share a `pointerId` with an overlapping eligible button, so the button can stay pressed while the drag continues.
- A centered eligible button now extends the drag-start area to its full touch radius, so presses on the outer half of the button can still begin the drag.
- Button press, release, and long-press behavior continue to flow through the existing layout-button helpers instead of a parallel drag-only state machine.
- `ACTION_POINTER_UP` now releases every control owned by that pointer rather than assuming a pointer can only belong to one control.
- Added `TouchOverlayDragZonePolicyTest.kt` to cover the latch-eligibility policy and the centered-button drag-start extension rule.

### Validation so far

- Passed: `android\gradlew.bat :app:compileDebugKotlin`
- Passed: `android\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.TouchOverlayDragZonePolicyTest"`
- Passed: `android\gradlew.bat :app:testDebugUnitTest`
- Passed: `android\run-code-quality.ps1 -Fix`
- Passed: `run-windows-build.ps1 -Target both -Compiler vs2022-community`
- Remaining: by-hand phone/emulator verification with a button placed inside a look drag zone

### Validation

- On a phone, place "primary fire" inside the look drag zone, hold and drag: look continues, weapon fires continuously, release stops both.
- No regression when no button is present inside the drag zone.

---

## 6. In-game + launcher brightness (gamma) slider  [ ]

User requirement:
> in-game brightness adjustment that saves next to the af/msaa/etc. settings. also add a slider for it out of game (next to af/msaa/etc. in the graphics launcher sub menu).

### Anchors

- Engine-side brightness already exists:
  - [d2/main/config.h#L38](d2/main/config.h#L38) `int GammaLevel;` on `GameCfg`.
  - [d2/main/config.c#L56, #L143, #L240-L242, #L323, #L345](d2/main/config.c#L56) persistence via `GammaLevel=` in `descent.cfg`.
  - [d2/main/menu.c#L1289, #L1311-L1312, #L1398](d2/main/menu.c#L1289) Graphics Options menu already exposes a slider via `opt_gr_brightness`.
- Duplicate path in D1: `d1/main/config.c`, `d1/main/menu.c` (verify when implementing).
- Launcher graphics page: `GraphicsPage.kt` (hosts MSAA/AF sliders). Current config writer: search `msaa=`, `anisotropy=` in launcher writers.

### Proposed approach

- Launcher slider: add a Brightness slider alongside MSAA/AF on `GraphicsPage.kt`. Range matches engine (0..4 or whatever `opt_gr_brightness` allows; confirm the menu's `min_value`/`max_value` when implementing).
- Persistence: the launcher already edits `descent.cfg` for other values. Add `GammaLevel=` writing next to the MSAA write, sharing the existing config-editing path in C through a new JNI helper if one does not already exist. Prefer calling a C helper (single source of truth per project rules).
- In-game slider: add a new admin tray row "Brightness" below MSAA/AF entries in `TouchOverlayView.kt`. Dispatch via `adminTrayCallback` to a new action id that calls `gr_palette_set_gamma(level)` and updates `GameCfg.GammaLevel`. Persist on admin-tray close by reusing the existing settings-save flow.

### Validation

- In-game admin tray slider visibly changes gamma immediately and the setting survives a restart.
- Launcher slider writes `GammaLevel=` to `descent.cfg`; launching the game picks it up at startup.
- Windows/Linux/Mac unaffected: launcher changes are Android-only; engine changes are `#ifdef __ANDROID__` only where needed (the core `GammaLevel` already exists on all platforms).

---

## Work order

1. Item 1 (controls editor long-press) - implement now, with unit tests.
2. Items 2-6 can be scheduled independently; each section above has the anchors needed to start without re-surveying.
