# Plan: Touch Editor Fixes -- Top Blocking, Floating Zones, Gyro Recenter

## TL;DR
Fix 6 issues with the touch control editor: (1) touches blocked at top of screen because system bars aren't hidden, (2) no UI to edit floating zone bounds, (3) floating zones default to left-half when added, (4) floating zones should default to a region around the stick, (5) add sensible default floating zones to presets with analog sticks, (6) add a gyro recenter button type.

## Root Cause Analysis

### Issue 1: Top-of-screen touches blocked
The touch editor runs inside SetupActivity which uses edge-to-edge (`setDecorFitsSystemWindows(false)`) but does NOT hide system bars. In landscape mode, the status bar and/or navigation bar remain visible and consume touch events. The BottomSheetScaffold's `innerPadding` shifts the canvas down by the status bar inset height, creating a dead zone at the top where controls appear (via percentage positioning) but touches are intercepted by the system bar.

**Fix**: Hide system bars when TouchEditorPage is active (same as MainActivity does for the game), using `WindowInsetsControllerCompat.hide(systemBars())` with `BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE`. Restore on editor close.

### Issue 2: No UI to edit floating zone bounds
The StickPropertiesPanel only has a `Floating` toggle. The `FloatingZone.leftPct/topPct/rightPct/bottomPct` values can only be changed via JSON export/import.

**Fix**: Add 4 percentage sliders (Left, Top, Right, Bottom) shown conditionally when `floating = true`.

### Issue 3: Floating zones default to left half
`FloatingZone()` constructor defaults to `(0, 0, 50, 100)` -- always the left half.

### Issue 4: Floating zones should default around the control
When floating is toggled on or a new floating stick is added, the zone should center itself around the stick's current position with reasonable padding.

**Fix**: When toggling `floating` from false to true, compute:
- leftPct = max(0, stick.xPct - 20)
- rightPct = min(100, stick.xPct + 20)
- topPct = max(0, stick.yPct - 30)
- bottomPct = min(100, stick.yPct + 30)
Apply this as the initial floatingZone.

### Issue 5: Default floating zones in presets with analog sticks
`simple.json` and `advanced.json` have analog sticks but `floating: false` with default zones of `(0, 0, 50, 100)`. The `claw.json` preset has sensible floating zones.

**Fix**: Enable floating on the primary (move) stick in `simple.json` and both sticks in `advanced.json`, with zones appropriate for each stick's position.

### Issue 6: Gyro recenter button
No action type exists for recentering the gyro. The gyro already has `calibrate()` which resets the reference orientation.

**Fix**: Add a new virtual binding `BTN_GYRO_RECENTER = 101` (overlay-only, next to `BTN_CHEATS_MENU = 100`). Handle in `TouchOverlayView` like `BTN_CHEATS_MENU` -- intercept on press, call `gyroManager?.calibrate()`. No JNI or C-side changes needed.

## Steps

### Phase A: System bar hiding in touch editor (Issue 1)

1. **TouchEditorPage.kt** -- In the `DisposableEffect` (~line 77), add system bar hiding using `WindowInsetsControllerCompat`. Hide on enter, restore on dispose. Same pattern as MainActivity.kt lines 411-414.

### Phase B: Floating zone editing UI (Issues 2, 3, 4)

2. **TouchEditorPage.kt StickPropertiesPanel** (~line 1159) -- After the `Floating` toggle, add conditional block when `floating == true`: 2 rows of 2 `LabeledSlider` (Left%/Right%, Top%/Bottom%).

3. **TouchEditorPage.kt StickPropertiesPanel** -- Change the `Floating` toggle handler to auto-compute a default zone when enabling floating: +/-20% horizontal, +/-30% vertical from stick position, clamped to 0-100%.

### Phase C: Preset floating zone defaults (Issue 5)

4. **simple.json** -- Enable floating on move stick with zone `(0, 40, 40, 100)`.

5. **advanced.json** -- Enable floating on both sticks: move `(0, 40, 40, 100)`, look `(60, 40, 100, 100)`.

### Phase D: Gyro recenter button (Issue 6)

6. **TouchBindings.kt** -- Add `BTN_GYRO_RECENTER = 101` and label `"Gyro Recenter"`.

7. **TouchOverlayView.kt** -- Handle `BTN_GYRO_RECENTER` on press: call `gyroManager?.calibrate()`. No-op on release.

8. **claw.json** -- Add a Gyro Recenter button at ~(50%, 92%), small size.

## Relevant Files

- TouchEditorPage.kt -- System bar hiding, floating zone sliders, auto-compute zone
- TouchBindings.kt -- BTN_GYRO_RECENTER constant and label
- TouchOverlayView.kt -- Handle BTN_GYRO_RECENTER
- simple.json -- Enable floating on move stick
- advanced.json -- Enable floating on both sticks
- claw.json -- Add gyro recenter button
- GyroInputManager.kt -- Already has calibrate(), no changes needed

## Verification

1. Build, install, open touch editor -- no status bar, touches at top work
2. Select stick, toggle Floating on -- zone auto-computes, 4 sliders appear
3. Adjust sliders -- blue zone rectangle updates
4. Load simple/advanced presets -- floating enabled with correct zones
5. Load claw preset -- gyro recenter button visible
6. In-game: tap gyro recenter -- view recenters
7. Run `android\run-code-quality.ps1 --fix`

## Decisions

- Gyro recenter uses overlay-only binding 101 (next to BTN_CHEATS_MENU = 100) -- acts entirely in Kotlin, no JNI/C changes
- Floating zone auto-defaults: +/-20% horizontal, +/-30% vertical from stick position
- simple.json gets floating on its 1 stick; advanced.json on both sticks
