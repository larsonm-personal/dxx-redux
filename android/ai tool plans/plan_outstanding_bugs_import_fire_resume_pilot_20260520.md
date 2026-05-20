# Outstanding Bugs: Import Dialog, Fire Secondary, Resume Panel, Select Pilot

## Scope

This plan covers these open items from `android/outstanding_bugs.md`:

- `Import Disc Image` action buttons should appear at the top of the dialog
- `Import Disc Image` action buttons should be reachable by D-pad on open
- `Import Disc Image` should show scroll indicator icons
- `Fire Secondary` bound to a controller button can fire all remaining ammo on a tap
- `Resume Recent Save` should occupy only the left column in landscape launcher layouts
- `Select pilot` native listbox title overlaps the pilot list

## Research Status

- [x] Locate launcher import dialog layout, focus, and scroll code
- [x] Locate launcher resume panel placement and landscape split layout
- [x] Trace controller button input from Android events into native controls
- [x] Trace native secondary-fire count/state consumption
- [x] Locate D1 and D2 pilot listbox call sites and title strings
- [x] Inspect native listbox measurement and draw code
- [x] Implement fixes
- [x] Run targeted tests and code quality checks

## Status Update 2026-05-20

- Phase 1 completed in `SetupActivity.kt`: moved the Import Disc Image primary actions to the top, removed the bottom confirm-button focus trap, and added `ScrollArrows(scrollState)`
- Phase 2 completed in `SetupActivity.kt`: the resume panel now renders inside the left/files column in landscape while keeping the portrait placement unchanged
- Phase 3 investigated with a new Android automation script: `android/game_scripts/test_fire_secondary.json5` confirmed that a one-frame tap on mixer button `101` spends exactly one concussion missile on a fresh D2 default binding, so no code change was needed for this item in the current tree
- Phase 4 completed in both `d1/main/newmenu.c` and `d2/main/newmenu.c`: preserved measured listbox title height and expanded the owned listbox window height to include the title area
- Validation completed with `:app:externalNativeBuildDebug`, `android/run_test.ps1 test_fire_secondary.json5 -Game d2`, `run-windows-build.ps1 -Target both`, and `android/run-code-quality.ps1 -Paths ...`

## Key Files And Functions

Launcher UI:

- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
  - `DiscImportDialog()`
  - `IsoImportDialog()` for comparison if matching behavior is needed
  - `SetupScreen()`
  - `ResumeSavePanel()`
  - `ScrollArrows()`

Controller input:

- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
  - `onKeyDown()`
  - `onKeyUp()`
  - `shouldDispatchGamepadButtonDown()`
  - `shouldDispatchGamepadButtonUp()`
  - `gamepadButtonIndex()`
  - `dispatchDpad()`
- `android/app/src/main/java/com/dxxredux/app/InputMixer.kt`
  - combined source/button state dispatch
- `android/app/src/main/cpp/android_input.c`
  - `nativeJoystickButton()`
- `android/app/src/main/cpp/android_gamepad_config.cpp`
  - `android_reload_live_gamepad_config()`
- `android/app/src/test/java/com/dxxredux/app/GamepadButtonDispatchPolicyTest.kt`
- `android/app/src/test/java/com/dxxredux/app/GamepadButtonEdgeTrackerTest.kt`

Native fire controls:

- `d1/main/gamecntl.c`
  - `do_weapon_n_item_stuff()`
- `d2/main/gamecntl.c`
  - `do_weapon_n_item_stuff()`
- `d1/main/kconfig.c`
- `d2/main/kconfig.c`

Native pilot listbox:

- `d1/main/menu.c`
  - `RegisterPlayer()`
  - `player_menu_handler()`
- `d2/main/menu.c`
  - `RegisterPlayer()`
  - `player_menu_handler()`
- `d1/main/text.h`
  - `TXT_SELECT_PILOT`
- `d2/main/text.h`
  - `TXT_SELECT_PILOT`
- `d1/main/newmenu.c`
  - `struct listbox`
  - `listbox_create_structure()`
  - `listbox_draw_contents()`
  - `listbox_draw()`
  - `android_listbox_draw_scaled()`
  - `listbox_handler()`
  - `newmenu_listbox1()`
- `d2/main/newmenu.c`
  - same listbox functions
- `d2/2d/font.c`
  - `gr_get_string_size()`
  - `gr_string()`
  - `ogl_internal_string()`

## Findings

### Import Disc Image Layout, Focus, And Scroll

`DiscImportDialog()` currently renders its action buttons after the CUE/BIN names, status, and track list. The requested behavior is to move the primary actions above that informational content.

The dialog already has `FocusRequester`s for `extractFocus`, `addAudioFocus`, and `doneFocus`, plus a `LaunchedEffect()` that requests the best action based on parsed tracks and processing state. However, `Close` is still in `AlertDialog.confirmButton`, and that likely gets the initial D-pad focus before the content action request stabilizes.

The dialog uses an unnamed `rememberScrollState()` inside `Column(modifier = Modifier.verticalScroll(...))`, so it cannot currently reuse the existing `ScrollArrows(scrollState)` helper. The fix should name the scroll state and wrap the scrollable text/body in a `Box`, following the launcher pattern already used for the main setup panes.

### Resume Recent Save Landscape Placement

`SetupScreen()` renders `ResumeSavePanel()` above the landscape `Row` that contains the left files pane and right controls pane. This makes the resume card span the whole screen in landscape. The likely fix is to render the panel as part of the left/files column in landscape, while keeping the current top-of-content placement for portrait.

Avoid duplicating the resume callbacks. Prefer a small local composable lambda inside `SetupScreen()` for the panel block, then call it from the correct layout branch.

### Fire Secondary Controller Over-Fire

This is the riskiest item and should be reproduction-first. A raw controller-button gate has broken native menu/game behavior before, and existing notes say stale live joystick bindings were a prior root cause of B-button over-fire.

Observed path:

- Android `MainActivity` receives physical button down/up
- `InputMixer` sends mapped virtual button state changes only when the combined state changes
- `MainActivity` also dispatches raw joystick button events so native menus still work
- `android_input.c` injects SDL joystick button events
- `kconfig.c` maps joystick button down/up into `Controls.fire_secondary_state` and `Controls.fire_secondary_count`
- `gamecntl.c` consumes secondary fire by adding `Controls.fire_secondary_count`, and while `Controls.fire_secondary_state` remains true it can enqueue one missile per frame through `Global_missile_firing_count`

The most plausible failure modes are:

- The native `Controls.fire_secondary_state` sticks true because a button-up path is missed
- The physical raw button and the virtual mapped button are both bound to Fire Secondary in active native config
- The active `PlayerCfg.KeySettings[1]` or derived joystick mapping is stale after launcher config changes

Do not start by suppressing raw button events globally. First confirm the active native mapping and the down/up transition seen by `kconfig.c`.

### Select Pilot Title Overlap

D1 and D2 both call:

```c
newmenu_listbox1(TXT_SELECT_PILOT, NumItems, m, allow_abort_flag, citem, ...)
```

Both games define `TXT_SELECT_PILOT` as a two-line string:

```c
"Select pilot\n<Ctrl-D> deletes"
```

`gr_get_string_size()` handles embedded newlines by increasing the measured height, so the font measurement function is not the main suspect.

The stronger suspects are listbox geometry and title-height lifetime in both `d1/main/newmenu.c` and `d2/main/newmenu.c`:

- `listbox_create_structure()` measures the title into `lb->title_height`
- later row-height measurements pass `&lb->title_height` as the `average_width` output scratch value, clobbering the measured title height
- `newmenu_listbox1()` creates the window with height `lb->height + 2 * BORDERY`, which does not include `lb->title_height`
- The Android scaled-listbox path builds a source rectangle that does include the title area above `lb->box_y`

This can shrink the reserved title space and make the Android path draw or scale a title area that the window region does not own, leading to overlap or clipping. Fix both games by preserving `lb->title_height`, using a throwaway `average_width` variable for row metrics, and making the owned window/source geometry agree.

## Work Plan

### Phase 1: Import Disc Image Dialog

- Move the Extract/Add Audio/Done action row to the top of `DiscImportDialog()` body, before file names and track details
- Keep disabled or absent states consistent with the current logic for data/audio availability
- Remove or demote `Close` so it does not trap initial D-pad focus on open
- Make the first available primary action request focus after parsing completes
- Add a named scroll state and wrap the body in a `Box` with `ScrollArrows(scrollState)`
- Recheck long CUE/BIN lists and small landscape heights

### Phase 2: Resume Recent Save Placement

- Extract the existing resume-panel `AnimatedVisibility` block in `SetupScreen()` into a small local composable lambda
- In landscape, render that lambda at the top of the left/files pane
- In portrait, keep rendering it above the stacked content
- Preserve the existing hide, stop-showing, and launch behavior
- Verify no additional full-width spacer remains in landscape

### Phase 3: Fire Secondary Diagnosis And Fix

- Reproduce with a controller binding where Fire Secondary is on a physical button
- Before changing routing, inspect active native config after launch, especially `PlayerCfg.KeySettings[1]` and joystick slots for raw button numbers vs virtual `100+` indices
- If needed, add temporary Android-port debug logging around native joystick button down/up, `kconfig.c` Fire Secondary updates, and `do_weapon_n_item_stuff()` ammo consumption
- Confirm whether the failure is duplicate down paths, missing up, or stale live config
- Fix at the narrowest confirmed layer
- Extend `GamepadButtonDispatchPolicyTest` or `GamepadButtonEdgeTrackerTest` only if Kotlin routing changes are needed
- If the fix is native config/state, add a high-level Android automation script that fires one secondary press and checks ammo delta via introspection

### Phase 4: Select Pilot Native Listbox

- Apply the fix in both `d1/main/newmenu.c` and `d2/main/newmenu.c`
- Keep changes minimal and style-matched for upstreamability
- Ensure `lb->title_height` stores the measured title height and is not reused as an output scratch variable
- Include `lb->title_height` in the listbox window height or otherwise make the Android scaled source rectangle match the owned draw region
- Verify the same path still handles one-line listbox titles and empty titles
- Prefer a geometry fix over a special case for `TXT_SELECT_PILOT`

### Phase 5: Validation

- Run `android\stop-stale-formatters.ps1` before any formatter or cleanup pass
- Run focused JVM tests for any Kotlin controller/UI logic touched
- Run `android\run-code-quality.ps1 --fix` after implementation
- Run a Windows host build with `run-windows-build.ps1` if D1/D2 native files change
- Run an Android debug build if launcher/native Android bridge files change
- For UI behavior, use launcher automation or setup introspection where possible, then manual TV/phone verification for D-pad focus and visual layout
- For Fire Secondary, validate with a one-button tap and check ammo delta through introspection rather than screenshots

## Implementation Notes

- Treat the launcher UI fixes as localized Compose changes in `SetupActivity.kt`
- Treat Fire Secondary as high risk because raw controller events are still needed by native menus
- Treat Select Pilot as a mirrored D1/D2 native fix, not an Android-only text workaround
- Keep any temporary fire-debug instrumentation demo-agnostic and remove it before finishing unless it becomes a generally useful Android debug log