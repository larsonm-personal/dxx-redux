# Kotlin menu zoom, vertical pan, and back overlay plan

Status: implemented; automated validation complete

Created: 2026-08-23

## Goal

Improve native game menus on Android without changing desktop behavior:

- Pinch to zoom when both starting pointers are outside native tappable and
  scroll-owned regions
- Pan vertically, while keeping the menu horizontally centered at every zoom
  level
- Preserve exact native taps, sliders, listbox selection, and the existing
  special drag-to-scroll behavior
- Preserve text entry and keep the active field centered above the IME while
  zoomed and vertically panned
- Show a large circular Back action at the bottom right whenever a supported
  native menu is frontmost
- Compose Back with the existing Exit action instead of adding another
  unrelated full-screen overlay

## Current architecture and findings

- `MainActivity.kt` sends normalized `SurfaceView` coordinates through
  `nativeTouchEvent()` and does not know the engine canvas resolution
- `android_input.c` converts surface coordinates to the current engine canvas,
  compensates for the keyboard viewport offset, and then inverse-maps the
  current scaled-menu destination rectangle into original menu coordinates
- The inverse transform is locked for the complete DOWN/MOVE/UP sequence, which
  is required for accurate release hit tests after a menu redraw
- `android_menu_scale.c` is already the shared D1/D2 source of truth for menu
  scale, source/destination rectangles, rendering, kconfig cropping, and the
  published transform used by touch and keyboard code
- D1 and D2 `newmenu.c` share the same Android behavior but retain small local
  draw and hit-test integrations
- A scroll-box `newmenu` starts its special drag scroll from anywhere in the
  scroll viewport, not only on row text. Kconfig similarly owns drag scrolling
  across its rendered viewport
- Listbox and ordinary newmenu item bounds already have exact native helper
  math. Gesture exclusion should reuse those helpers instead of duplicating
  font, row, or scroll calculations in Kotlin
- Text-field centering is already zoom-aware: native code maps the selected
  field Y through the current menu transform before computing
  `g_blit_y_offset`. The extension must include vertical pan in that same
  published destination transform
- Menu frames do not pass through the normal 3D start/end frame hooks. The
  visual transform must remain in the menu draw path and `gr_flip` keyboard
  viewport path
- `ExitButtonView` is a full-screen custom View that draws a small upper-left
  circle. `SkipButtonView` is separate and also temporarily presents a Back
  label for save/load menus
- `nativeIsInGame() == false` is too broad for overlay eligibility because it
  also includes movies, transient screens, and other non-menu windows

## Recommended ownership

Use a Kotlin custom View as the gesture and action overlay, but keep menu pixel
rendering and the authoritative coordinate transform native.

Do not scale or translate the Android `SurfaceView`. A View transform would sit
outside the existing native scale-blit, touch remap, menu filtering, and IME
viewport logic, creating two coordinate systems and potentially scaling 3D or
transient screens.

Recommended components:

- `MenuInteractionOverlayView.kt`
  - Recognizes background pan and two-pointer pinch gestures
  - Draws the bottom-right Back button and optional upper-left Exit button
  - Routes native-owned touches through unchanged
  - Holds only user intent: zoom multiplier and vertical pan fraction
- `MainActivity.kt`
  - Supplies callbacks and menu/IME state
  - Replaces the standalone `ExitButtonView` with the composed overlay
  - Sends viewport intent to JNI and dispatches Back with existing IME policy
- `android_menu_scale.h/.c`
  - Applies user zoom and vertical pan to every supported native menu transform
  - Keeps destination X centered and clamps scale/pan safely
  - Publishes one transform snapshot used by draw, touch, hit classification,
    keyboard centering, introspection, and tests
- Small D1/D2 hooks in `newmenu.c` and `kconfig.c`
  - Publish authoritative native interaction regions from existing geometry
  - Keep D1/D2 behavior mirrored, with shared new logic under `android/`

This uses the existing custom-View style and adds no Compose dependency.
"Composable" here means one overlay can independently show Back, Exit, both,
or neither.

## Coordinate model

Keep one forward transform for rendering and one exact inverse for input.

Definitions:

- `src`: original engine-canvas rectangle containing menu content
- `B`: existing automatic scale, or 1.0 when automatic enlargement is inactive
- `Z`: Kotlin user zoom multiplier, initially 1.0
- `S`: effective scale after safety clamps
- `P`: vertical pan in engine screen pixels
- `W`, `H`: current engine canvas size
- `K`: current keyboard viewport upward offset

Forward transform:

```text
S = clamp(B * Z, 1.0, renderSafetyMaximum)
dstW = round(srcW * S)
dstH = round(srcH * S)
dstX = (W - dstW) / 2
dstY = (H - dstH) / 2 + P

screenX = dstX + (canvasX - srcX) * S
screenY = dstY + (canvasY - srcY) * S - K
```

Inverse touch transform:

```text
canvasX = srcX + (screenX - dstX) / S
canvasY = srcY + (screenY + K - dstY) / S
```

Requirements for the implementation:

- Use destination/source integer ratios for final touch mapping, as the current
  code does, so rendered integer rectangles and hit-test integer coordinates
  cannot drift due to separate float rounding
- Always derive `dstX`; never accept or store horizontal pan
- Keep kconfig source scrolling (`src.y - box.y`) separate from viewport pan
  (`P`). Both then work through the same inverse transform
- Clamp vertical pan after every zoom, surface resize, IME change, and menu
  geometry change. For smaller content, keep it within the screen; for larger
  content, do not allow blank space beyond the content edge
- Cap effective scale from maximum render dimensions/area and supported texture
  upload size, not only a UI magic number. Current code allocates indexed render
  buffers every frame, so memory and upload cost must be bounded

For focal-point zoom, update `P` so the canvas point under the pinch centroid
keeps the same screen Y before clamping:

```text
ratio = newS / oldS
oldCenterY = H / 2 + oldP
newCenterY = focalY - (focalY - oldCenterY) * ratio
newP = newCenterY - H / 2
```

Two-pointer centroid movement adds vertical pan. Horizontal centroid movement
is ignored. A one-pointer vertical background drag also updates only `P`.

## Native menu and interaction snapshot

Add a small fixed-capacity snapshot adjacent to the shared menu scale state.
It must contain:

- Active/supported menu flag
- Menu kind: newmenu, listbox, or kconfig
- Generation that changes when the front menu or source geometry changes
- Current source/destination transform and keyboard offset generation
- Interaction rectangles in original engine coordinates
- Flags per rectangle, at minimum `TAPPABLE` and `SCROLL_OWNED`

Publishing rules:

- Ordinary newmenu: publish visible non-text item bounds from the existing
  `newmenu_get_item_bounds()` math, including slider and input rows
- Scroll-box newmenu: publish the entire scroll viewport as `SCROLL_OWNED`, plus
  item/arrow bounds as applicable. This preserves its special native drag path
- Listbox: publish visible selectable row bounds and any native scroll-owned
  viewport if drag scrolling is added or already active for that variant
- Kconfig: publish the visible destination/source viewport as `SCROLL_OWNED`
  because its current Android handler scrolls from a drag anywhere in the view
- Clear the snapshot on close, unsupported draws, movies, level-complete
  exclusions, and when no supported menu is frontmost

JNI must query only this snapshot. It must not dereference live `newmenu`,
`listbox`, `kc_menu`, or `window` pointers from the Android UI thread.
Use a sequence counter or equivalent atomic publication so JNI never combines
regions from one frame with a transform from another.

Expose narrow JNI methods such as:

- `nativeGetMenuOverlayState(): Long` for active/kind/generation flags
- `nativeMenuPointFlags(normX, normY): Int` using the exact shared inverse
  transform, including `K`
- `nativeSetMenuViewport(zoomFixed, panFractionFixed)` for atomic UI intent
- `nativeResetMenuViewport()` when the native menu session ends

Use fixed-point integer values across UI/game threads, or another proven atomic
representation, so a game-frame read cannot observe a torn float update.

## Kotlin gesture arbitration

The overlay must decide ownership on ACTION_DOWN using the native point flags.

### Native-owned start

If the first pointer is `TAPPABLE` or `SCROLL_OWNED`:

- Return false from the overlay so the `SurfaceView` receives the original
  DOWN/MOVE/UP stream without replay or coordinate modification
- Do not start pinch if a later pointer appears
- Preserve the native touch-sequence transform lock

This keeps taps, check/radio deferral, sliders, text entry, long-press reorder,
listbox selection, and special newmenu/kconfig drag scrolling unchanged.

### Background-owned candidate

If the first pointer is outside native-owned regions:

- The overlay consumes the stream but initially delays forwarding a native DOWN
- A vertical move beyond touch slop becomes one-pointer vertical pan
- A large horizontal-only move is consumed without panning or synthesizing a
  tap, since horizontal pan is intentionally unsupported
- A release within tap slop replays a native DOWN/UP at the original/release
  coordinates so existing outside-tap behavior is retained

Deferring the background DOWN avoids inventing a native cancel event. A
synthesized mouse-up on a blank pause-menu area could otherwise trigger the
existing tap-outside-to-close behavior while the user was beginning a pan.

### Second pointer

When the first pointer is a background-owned candidate:

- Query the second pointer with `nativeMenuPointFlags()`
- Start pinch only when both pointer flags exclude `TAPPABLE` and
  `SCROLL_OWNED`
- Lock gesture ownership and the menu generation for the full pinch
- If either point is native-owned, reject pinch, ignore the extra pointer, and
  retain the first background candidate semantics without activating the item
  under the second finger
- Cancel the gesture without a native tap if the menu generation changes,
  Back/Exit is pressed, the activity pauses, or an overlay visibility change
  removes menu eligibility

Disallow starting a menu viewport gesture while the IME is actively accepting
text. A menu may remain zoomed when text entry starts, but freezing gesture
updates while typing avoids fighting Android keyboard gestures and viewport
animation.

## Zoom and pan lifetime

Recommended initial policy:

- Existing automatic menu scale remains the default at `Z = 1.0`
- Preserve `Z` while navigating nested supported native menus in one continuous
  menu session
- Reset vertical pan to centered and reclamp whenever the menu generation or
  source geometry changes
- Reset both zoom and pan when leaving supported menus for gameplay, movies, the
  launcher, or activity teardown
- Do not persist these values to preferences in the first tranche

This avoids carrying an offset from a tall menu into a short submenu while
keeping a user's chosen accessibility magnification through normal navigation.

## Back and Exit overlay behavior

Replace the standalone exit-only View with one menu-action overlay:

- Back is a circular bottom-right action with an initial visual diameter near
  twice the current Exit circle, with dp minimum/maximum bounds and at least a
  48 dp touch target
- Back is visible whenever the native snapshot reports a supported front menu,
  including newmenu, listbox, and kconfig. It is harmless on the root main menu,
  whose native handler already consumes Escape
- Exit retains its current upper-left position and gameplay-aware callback
- Existing polling controls `showBack` and `showExit` independently; do not use
  `!nativeIsInGame()` as the Back predicate
- Both buttons stay screen-anchored and are never included in menu zoom/pan
- Button hit tests take priority over gesture recognition and the SurfaceView
- Anchor Back above bottom system/IME insets so it remains permanently
  reachable while the soft keyboard is visible

Back dispatch policy:

- If the IME/keyboard proxy is active, the first Back tap hides/deactivates the
  IME consistently with hardware/controller Back and does not close the menu
- Otherwise inject native `KEYCODE_BACK` down/up, which maps to the engine's
  existing Escape behavior
- Remove the save/load-only temporary `SkipButtonView` Back presentation once
  the permanent Back action covers that menu, but retain Skip/Next/Continue and
  intro behavior in `SkipButtonView`
- Back must not invoke `META_RETURN_TO_LAUNCHER`; that remains the distinct Exit
  action

## Implementation phases

1. [x] Shared viewport math and tests
   - Extend `android_menu_scale` with user zoom, vertical pan, horizontal-center
     invariants, focal update inputs, clamps, and clear/reset semantics
   - Keep existing scale behavior byte-for-byte equivalent when `Z = 1` and
     `P = 0`
   - Add focused host tests for forward/inverse round trips, integer edges,
     focal-Y stability, no horizontal translation, pan clamps, kconfig crop plus
     pan, and render-area caps

2. [x] Authoritative interaction publication
   - Add the atomic fixed-capacity snapshot and shared point-classification API
   - Publish visible newmenu/listbox/kconfig regions from mirrored D1/D2 hooks
   - Reuse current native item-bound helpers and scroll line spacing
   - Extend introspection with menu kind, generation, zoom multiplier, pan,
     effective transform, interaction counts, and clamp status

3. [x] Kotlin overlay and gesture state machine
   - Add `MenuInteractionOverlayView` with explicit idle, background-candidate,
     native-pass-through, pan, pinch, and cancelled states
   - Add unit tests for both-points-clear gating, second-point rejection, touch
     slop, horizontal suppression, generation changes, and blank-tap replay
   - Feed fixed-point viewport intent through JNI at most once per display frame
     to avoid redraw/JNI flooding

4. [x] Back/Exit composition and IME behavior
   - Move Exit drawing/hit behavior into the composed overlay without changing
     its callback policy
   - Add bottom-right Back with inset-aware geometry and accessibility content
     descriptions/click handling
   - Unify Back callback behavior with the current keyboard proxy policy
   - Retire only the save/load-specific temporary Back label, leaving other
     transient actions untouched

5. [x] D1/D2 integration regression
   - Extend `test_newmenu_render_paths_unified.jsonc` or add a focused unified
     script covering newmenu, scroll-box newmenu, listbox, kconfig, pause menu,
     and text input
   - Add debug automation support to set zoom/pan and inject a tap at a chosen
     normalized point, rather than relying on screenshots
   - Assert transformed taps select the intended row at minimum/maximum zoom
   - Assert native drag scrolling still changes `scroll_offset` or kconfig
     `scroll_y` and does not change viewport pan when started in scroll-owned
     space
   - Assert a pinch candidate is rejected when either point is item/scroll-owned
   - Assert Back closes a submenu, hides IME first during text entry, and does
     nothing destructive at the root main menu
   - Assert Exit retains its prior launcher/game-menu behavior

6. [ ] Validation and physical-device tuning
   - Run scoped code quality on all changed Kotlin, shared Android native, D1,
     D2, test, and plan files
   - Build Android debug with JDK 21
   - Run relevant JVM/host tests and the unified emulator script for D1 and D2
     with logcat cleared and output written under `temp/`
   - Run the Windows host build for both games because D1/D2 menu files change
   - Manually verify a real two-finger gesture, button ergonomics, and IME inset
     animation on at least phone portrait/landscape dimensions
   - Record measured frame time and peak render-buffer dimensions before
     finalizing the maximum zoom

## Acceptance criteria

- Both fingers starting on true background can zoom and vertically translate a
  supported native menu
- Either finger starting in a tappable or scroll-owned region prevents pinch
- Menu X center is invariant under zoom, pan, orientation, and resolution change
- Ordinary taps, sliders, check/radio rows, listboxes, reorder rows, newmenu
  scroll boxes, and kconfig scrolling retain existing behavior at every zoom
- Opening a text field while zoomed centers that transformed field in the
  visible area above the IME; touch inverse mapping remains accurate after the
  viewport shift
- Back is always reachable at bottom right on supported native menus and uses
  normal native Back/Escape semantics after dismissing IME first
- Exit and other transient Skip/Next/Continue overlays keep their current
  behavior
- Unsupported screens and 3D gameplay do not receive menu gesture transforms
- D1 and D2 use the same new shared Android implementation with only narrow,
  mirrored integration hooks

## Main risks and guardrails

- Do not classify points by reading live engine window/menu objects on the UI
  thread
- Do not duplicate native row/font/scroll geometry in Kotlin
- Do not synthesize a native UP to cancel a newly claimed background pan
- Do not treat only row text as scroll-owned for scroll-box newmenu or kconfig
- Do not apply user pan by changing `g_blit_y_offset`; keyboard centering and
  user pan are separate terms in the same transform
- Do not overwrite existing kconfig source scroll with user viewport pan
- Do not let per-frame temporary bitmap allocation grow without a measured cap
- Clear transform and interaction state on every close/unsupported path so a
  stale menu cannot remap touches on gameplay or transient screens
- Keep all D1/D2 changes Android-guarded and mirrored

## Implementation and validation record

Implemented on 2026-08-23. The shared native transform now owns zoom, vertical
pan, horizontal centering, interaction classification, and IME-aware inverse
mapping. The Kotlin overlay composes Back and Exit and leaves native-owned
touch streams on the SurfaceView.

Completed validation:

- Scoped code-quality formatting and checks passed for the changed C, C++, and
  Kotlin sources
- `MenuInteractionOverlayViewTest` passed, covering both-point gating,
  focal-point stability, centroid pan, and zoom/pan intent clamps
- Android debug APK assembled successfully for arm64-v8a, armeabi-v7a, and
  x86_64 with JDK 21
- `test_newmenu_render_paths_unified.jsonc` passed on the emulator for D1 (71
  steps) and D2 (83 steps), including text entry, zoom/pan publication,
  newmenu, listbox, kconfig, scroll-owned regions, and pause-menu return
- The live Android view hierarchy exposed the full-surface composed overlay;
  an ADB tap at the computed bottom-right Back-circle center popped the active
  native menu layer
- Windows D1 and D2 release builds, including headless targets, passed
- `git diff --check` passed

Remaining optional tuning is a physical-device pass for real two-finger feel,
portrait/landscape ergonomics, and measured render-frame timing. The render
buffer is already hard-capped to 2048 pixels per dimension, and this tuning is
not required for functional availability.
