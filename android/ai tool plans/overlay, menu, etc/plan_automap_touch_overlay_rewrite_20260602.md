# Automap Touch Overlay Rewrite Plan

Status: implementation complete

Created: 2026-06-02

## Request

Phase 1:

- When automap is open, show the existing touch overlay instead of the current hard-coded automap overlay
- Trim the overlay while automap is active to movement controls only, plus the map button and menu/settings buttons
- Make automap movement rates closely related to normal mine movement rates
- Add one constant scale factor for automap translation so it can be tuned
- Keep look speed the same as normal mine look speed

Phase 2:

- Re-add marker controls through the touch overlay menu
- Make "jump to marker" and "recenter" active only while automap is open, as menu options

## Current Code Shape

- `MainActivity` polls `nativeIsAutomapActive()` and sets `touchOverlay.automapActive`
- `TouchOverlayView.onDraw()` currently exits early when `automapActive` and calls `drawAutomapOverlay()`
- `TouchOverlayView.onTouchEvent()` currently exits early when `automapActive` and calls `handleAutomapOverlayTouch()`
- That hard-coded automap overlay draws:
  - center button
  - one button per marker
  - enlarged MAP button
  - help text
- There are two separate automap gesture paths today:
  - `MainActivity.handleAutomapTouch()` for automap touches when the overlay is off
  - `TouchOverlayView.handleAutomapOverlayTouch()` and `handleAutomapMove()` for automap touches when the overlay is on
- `nativeAutomapInput()` writes Android-specific accumulated fix values into `g_automap_*`
- `d1/main/automap.c` and `d2/main/automap.c` merge those values in `automap_apply_input()`
- `automap_process_input()` already calls `kconfig_read_controls(event, 1)` and stores the result in `am->controls`
- Normal touch overlay axes flow through:
  - `TouchOverlayView.axisCallback`
  - `InputMixer.setAxis()`
  - `MainActivity.nativeJoystickAxis()`
  - SDL joystick event
  - `kconfig_read_controls()`
- Normal movement rates are computed in `kconfig_read_controls()` as `control_info` fields scaled by `FrameTime`, sensitivity, inversion, and deadzone state

## Phase 1 Plan

### 1. Replace automap drawing with filtered normal overlay drawing

Add an automap drawing path in `TouchOverlayView` that reuses the normal layout state but filters what is drawn:

- Draw analog movement controls only:
  - sticks
  - sliders
  - axis regions
  - dpads if they are movement-oriented
- Draw the automap/map button
- Draw settings/menu controls:
  - settings diagnostic/admin tray tab
  - more-actions button if it has automap-relevant actions
- Do not draw weapon wheels, fire buttons, cheat controls, music diagnostics, or non-movement combat buttons while automap is active

Suggested helper shape:

- `drawNormalOverlay(canvas, automapMode: Boolean)`
- `buttonVisibleInAutomap(binding: Int): Boolean`
- `radialVisibleInAutomap(...) = false` for phase 1
- `diagnosticVisibleInAutomap(...)` for settings only

Avoid a duplicate full drawing function. The current normal path already has the right geometry and visual style.

### 2. Replace automap touch handling with filtered normal overlay touch handling

Change `onTouchEvent()` so automap mode still uses the normal overlay input pipeline, but with the same automap filtering rules.

The desired behavior:

- Movement controls should keep sending normal joystick axes through `InputMixer`
- MAP button should still toggle automap
- Settings/menu controls should still open and operate
- Non-automap controls should be ignored but the overlay should still consume the touch so fire/mouse clicks do not leak to the game surface

Suggested helper shape:

- `handleOverlayTouch(event, automapMode: Boolean)`
- Filter each hit-test group before claiming/activating it
- Keep `return true` while overlay is active

This should make the current hard-coded `handleAutomapOverlayTouch()` path removable or at least unused in phase 1.

### 3. Route automap movement through existing `kconfig_read_controls()`

Keep touch movement input as normal virtual joystick axis input. Let `automap_process_input()` continue to call `kconfig_read_controls(event, 1)`, producing `am->controls` from the same rates as mine movement.

Then scale only translation in `automap_apply_input()`:

- Do not scale:
  - `heading_time`
  - `pitch_time`
  - `bank_time`
- Scale:
  - `forward_thrust_time`
  - `vertical_thrust_time`
  - `sideways_thrust_time`

Suggested C constant in both `d1/main/automap.c` and `d2/main/automap.c`:

```c
#ifdef ANDROID
#define ANDROID_AUTOMAP_TRANSLATION_SCALE 6
#else
#define ANDROID_AUTOMAP_TRANSLATION_SCALE 1
#endif
```

or, better for minimal scope:

```c
#ifdef ANDROID
#define AUTOMAP_ANDROID_TRANSLATION_SCALE 6
#endif
```

Apply it only around automap movement translation math or immediately after Android touch-derived controls are read from `kconfig`. Prefer not to affect keyboard/controller automap movement unless the product intent is that all Android inputs should move faster in automap.

Open implementation question:

- If the scale should affect only touch overlay axes, add an Android touch-source marker or separate automap movement injection path
- If the scale should affect all Android automap movement, the change is much simpler and can happen fully in automap.c

Recommended default: scale all Android automap translation inputs. It is simpler, consistent for gamepad-only Android, and avoids plumbing source metadata into `control_info`.

### 4. Remove or quarantine old hard-coded automap gesture path

Once normal overlay controls work in automap:

- Stop calling `drawAutomapOverlay()`
- Stop calling `handleAutomapOverlayTouch()`
- Stop assigning or using `automapInputCallback` for phase 1 movement
- Consider leaving `nativeAutomapInput()` in place until phase 2 is complete if recenter still uses the current JNI helpers
- Remove `MainActivity.handleAutomapTouch()` surface fallback only if overlay is guaranteed visible while automap is open

Be careful with overlay-disabled settings:

- The current polling path can show the overlay while automap is active through `shouldShowTouchOverlay()`
- Confirm that an explicit "overlay disabled" user preference does not bypass required automap controls

## Phase 2 Plan

### 1. Add automap-only menu actions

Add new admin or remaining-action IDs for:

- Automap recenter
- Jump to marker 1 through 10, D2 only

Preferred location:

- Remaining-actions menu, because it already supports dynamic action lists and contextual items
- It already accepts `RemainingTouchAction(label, adminAction = ...)`

Add a provider similar to current warp/join dynamic actions:

- `automapActionsProvider`
- Only returns actions when `automapActive == true`
- D1 returns recenter only
- D2 returns recenter plus existing marker count actions

### 2. Wire callbacks

Use existing native hooks where possible:

- Recenter: `nativeAutomapCenter()`
- Jump to marker: `nativeAutomapSelectMarker(idx)`

For labels:

- `Recenter Map`
- `Jump to Marker 1`, `Jump to Marker 2`, etc.

If the user specifically wants marker message text in labels later, add a D2 native provider for marker labels. Do not duplicate marker message parsing in Kotlin.

### 3. Retire top-row marker buttons

After menu actions exist:

- Remove marker button drawing from the automap overlay path
- Remove `markerCountProvider` usage from hard-coded button geometry
- Remove `automapCenterCallback` and `automapMarkerCallback` from direct overlay button handling if they are no longer used elsewhere

## Suggested Tests

Unit tests:

- Add policy tests for automap filtering:
  - movement controls visible
  - map button visible
  - settings/menu visible
  - weapon/fire controls hidden
- Add remaining-actions tests:
  - automap actions absent when automap inactive
  - recenter present when automap active
  - marker actions present only for D2 and only up to marker count
  - D1 does not expose marker jump actions

Integration or automation test:

- Extend or add an Android automation script under `android/game_scripts/`
- Launch D2 to a level, open automap, confirm overlay is active
- Move a touch movement control and introspect or log that automap control values were accepted
- Use MAP button to close automap
- Phase 2: open menu while automap is active and trigger recenter

Build/checks:

- Run targeted Kotlin unit tests if available for touched policy classes
- Run `android\run-code-quality.ps1 --fix`
- Run the Android debug build used by existing emulator tests
- If native `d1/` and `d2/` automap files change, run the normal Windows build helper or Android CMake/Gradle build as appropriate

## Risks

- Filtering hit-tests without duplicating the whole touch handler will need careful structure. The current handler is large and group-order dependent.
- If overlay-disabled users can open automap without `TouchOverlayView.isActive`, normal surface touches may still hit the old `MainActivity.handleAutomapTouch()` path unless it is removed carefully.
- Scaling translation in automap.c affects any Android input source unless extra source metadata is plumbed. That is probably acceptable, but it should be called out when implementing.
- D1 and D2 automap code are duplicated and must be changed together.

## Next Implementation Starting Point

Implementation completed in this tranche:

1. Added `AutomapTouchPolicy.kt` for automap button filtering and automap-only menu actions
2. Changed `TouchOverlayView.kt` so automap mode uses the normal overlay path with filtered buttons, normal movement axes, the map button, and menu/settings entry points
3. Removed the hard-coded automap overlay drawing and touch gesture path from `TouchOverlayView.kt`
4. Removed the surface-level automap gesture fallback and `nativeAutomapInput()` plumbing from `MainActivity.kt` and `android_input.c`
5. Routed recenter and D2 marker jump through automap-only remaining actions
6. Added `AUTOMAP_TRANSLATION_SCALE` in both `d1/main/automap.c` and `d2/main/automap.c`, applied only to automap translation fields while leaving look/bank timing unscaled
7. Added `AutomapTouchPolicyTest.kt` for trimmed button visibility, recenter, marker clamping, and marker action dispatch mapping

Validation completed:

- `android\run-code-quality.ps1 -Fix`
- `.\gradlew.bat --no-daemon --console=plain :app:assembleDebug`
- `.\gradlew.bat --no-daemon --console=plain :app:testDebugUnitTest --tests com.dxxredux.app.AutomapTouchPolicyTest`

Note: an earlier focused Gradle test run produced a passing XML result but the process did not unwind cleanly before the tool timeout. The no-daemon rerun above completed successfully.
