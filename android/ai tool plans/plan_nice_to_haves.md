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
- Optional by-hand: emulator with a BT controller, hold LS up on the controller editor, confirm the stick picker opens after 2s. Hold A, confirm the button picker opens.

### Out of scope

- No changes in `kconfig.c` or `kconfig.h`; we only reuse the existing dialog flow.
- No change to the selected-control highlighting behavior on the canvas.

---

## 2. "Prepare for Descent" loading progress bar  [ ]

User requirement:
> prepare for descent loading screen takes potentially up to ~5 seconds with high res textures. it would be nice to show a texture loading progress bar and maybe the text name of the most recently loaded texture or major step, or one text/bar update every 300ms max to limit drawing overhead.

### Anchors

- Message renderer: [d2/main/gamerend.c#L1061](d2/main/gamerend.c#L1061) `show_boxed_message()` and its D1 twin in `d1/main/gamerend.c`.
- Paging sweep (this is the slow loop): [d2/main/paging.c#L59+](d2/main/paging.c#L59) and the parallel file in `d1/main/paging.c`. Hot loops: texture frames, effects, model textures, wall anims, segments, powerups, weapons, cockpit gauges, bitmap files.
- Call site that prints "Prepare for Descent": search `TXT_PREPARE_FOR_DESCENT` in `d2/main/gameseq.c` / `d1/main/gameseq.c`.

### Proposed approach

Add a new Android-gated helper in `d2/misc/` (and duplicate for d1) or a shared helper under `android/app/src/main/cpp/` with both games' build including it:

- `void android_loading_progress_begin(int total);`
- `void android_loading_progress_step(const char *label);` increments done and stores most recent label, throttled internally to one flush per 300ms via `timer_query()`. Flush calls `show_boxed_message()` (or a bespoke lightweight render) to redraw the boxed message with an extra progress bar beneath and the label line above it.
- `void android_loading_progress_end(void);`

Instrument the paging sweep with `_begin(total)` before the outer loops and `_step(Textures[i].filename)` or similar inside the inner loops. Guard all of it with `#ifdef __ANDROID__` so Windows/Linux/Mac are unchanged.

### Validation

- Windows host build stays green (the `#ifdef __ANDROID__` guards keep it a no-op).
- On emulator, confirm the load screen shows a growing bar during the initial load of a big level.
- Manual throttle check: no visible stutter, updates no more than ~4/sec.

---

## 3. In-game Select button should open settings overlay, not save/load menu  [ ]

User requirement:
> need some fixes for android TV to transition all remaining touch overlay bits to controller interfaces. the immediate need is for the in-game settings menu (the overlay one) to be the thing opened by the select button, rather than the in-game save/load/quit menu (its current function). there was a task at one point to move extra items into the settings menu when there was no touch interface: that needs to be rechecked.

### Anchors

- Current Select mapping: [MainActivity.kt#L2285-L2300](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L2285-L2300). Select currently calls `openGameMenuSafely()` (the save/load/quit menu) and Start toggles the admin tray.
- `openGameMenuSafely()`: [MainActivity.kt#L1333](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L1333).
- Admin tray (overlay settings): in `TouchOverlayView.kt` around the `adminTray*` block starting at [TouchOverlayView.kt#L659-L683](android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt#L659-L683). `toggleAdminTray()` is called from the Start handler and dispatches through `adminTrayCallback` at [MainActivity.kt#L646-L656](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L646-L656).
- Earlier task that moved items into the tray when touch is disabled: search for "gamepadOnlyMode" in `TouchOverlayView.kt` admin-tray action list, and the action enum at [MainActivity.kt#L2115-L2116](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L2115-L2116).

### Proposed approach

- Swap Select and Start in the gamepad-only block: Select opens the admin tray; Start (or long-press Select, TBD) opens the save/load/quit menu.
- Audit the admin tray action list for completeness: confirm every action reachable from the in-game touch settings overlay is present in the tray when `gamepadOnlyMode` is true. Anything missing from the earlier task gets added.
- Update `docs/comments` note in `MainActivity.kt` near the Select handler so the mapping is clear.

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

## 5. Buttons inside drag zones with slide-to-release  [ ]

User requirement:
> allow buttons to be placed within the drag zones (they currently are allowed, but don't have special handling). if the button is pressed it stays active as long as the drag continues, then the button is released on drag stop. this will give another way to fire weapons while looking with the same thumb.

### Anchors

- Drag zone state: `TouchOverlayView.kt` mouse-mode / drag members near [TouchOverlayView.kt#L212-L240](android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt#L212-L240) and `beginMouseDrag` at [TouchOverlayView.kt#L444-L460](android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt#L444-L460).
- Buttons state list: the same file's `buttons` structure (search `b.pointerId` around [TouchOverlayView.kt#L994-L1000](android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt#L994-L1000)).
- Touch dispatch: `onTouchEvent` / `dispatchTouchEvent` in `TouchOverlayView.kt`.

### Proposed approach

- On `ACTION_DOWN` / `ACTION_POINTER_DOWN` inside a drag zone, first hit-test against any button whose rect overlaps the drag zone. If found: mark the button pressed, remember `pointerId -> (dragZone, buttonIndex)`, and start the drag with that pointer.
- While the pointer moves, the button stays latched-pressed and the drag accumulates normally.
- On `ACTION_UP` / `ACTION_POINTER_UP` / `ACTION_CANCEL` for that pointer: release the button and end the drag in the existing code paths.
- Multi-finger: each finger is independent; only the fingers that started on a button carry the button-press state.

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
