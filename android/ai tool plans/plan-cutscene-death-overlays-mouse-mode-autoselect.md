# Plan: Cutscene/Death Overlays, Mouse Mode, Autoselect Editor

## Feature A: Cutscene & Death Screen Overlay Controls

Show skip/continue button (instead of normal touch controls) during mine exit and player death.

### A1: JNI state queries
- In android_input.c, add nativeIsPlayerDead() returning Player_is_dead != 0 (include object.h)
- Add nativeIsEndlevelSequence() returning Endlevel_sequence != 0 (include endlevel.h)
- Declare matching external fun in MainActivity.kt
- Both variables exist identically in d1 and d2; JNI is in shared android_input.c

### A2: Customizable skip button text
- Add var label: String = "SKIP" to SkipButtonView.kt
- Use label in onDraw instead of hardcoded "SKIP"

### A3: Overlay switching logic
- Extend startOverlayPolling() in MainActivity.kt
- Query nativeIsPlayerDead() and nativeIsEndlevelSequence()
- Death: hide touch overlay, show skip button with label="CONTINUE"
- Endlevel: hide touch overlay, show skip button with label="SKIP"
- Priority: death > endlevel > skippable > automap > in-game
- ESC injection works for both (any key dismisses death, ESC dismisses endlevel)

### A4: Introspection
- Expose Endlevel_sequence in game_introspect.cpp (both d1/d2) for testability

### Files
- android/app/src/main/cpp/android_input.c
- android/app/src/main/java/com/dxxredux/app/MainActivity.kt
- android/app/src/main/java/com/dxxredux/app/SkipButtonView.kt
- android/app/src/main/cpp/shared/game_introspect.cpp

---

## Feature B: Mouse Mode for Touch Sticks

Add trackpad-like mouse mode as alternative to joystick mode for touch sticks.

### B1: Configuration
- Add mouseMode: Boolean = false to AnalogStickControl in TouchControl.kt
- Add mouseSensitivity: Float = 1.0f (single slider controls both delta-to-axis conversion and per-tick cap)

### B2: Touch handling
- In TouchOverlayView.kt, when mouseMode:
  - ACTION_DOWN in floating zone: record anchor position
  - ACTION_MOVE: compute pixel delta, convert to axis value (delta * sensitivity / refDistance)
  - Drag buffering: accumulate pending delta, emit clamped portion per tick, carry remainder
  - ACTION_UP: zero axis, clear buffer

### B3: Rendering
- When mouseMode: skip stick circle/thumb, draw only semi-transparent bounding box

### B4: Touch editor UI
- Add mouse mode toggle per stick in TouchEditorPage.kt
- Add sensitivity slider when mouse mode enabled
- Persist in overlay config JSON

### Design choice
- Drag buffering in Kotlin layer rather than engine mouse path
- Avoids modifying engine code; gives better control over touch-specific behavior

### Files
- android/app/src/main/java/com/dxxredux/app/TouchControl.kt
- android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt
- android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt

---

## Feature C: Launcher Autoselect Ordering Editor (deferred)

Full-screen Compose page for reordering weapon autoselect priority.

### C1: JNI C functions
- Create android_autoselect.cpp with:
  - nativeReadAutoselect(filesDir, game) -- reads first .plr, returns primary+secondary order
  - nativeWriteAutoselect(filesDir, game, primary, secondary) -- writes to ALL .plr files
  - nativeGetWeaponNames(game) -- hardcoded English names
  - nativeGetDefaultAutoselect(game) -- default ordering arrays
- D2: binary bytes at known offset. D1: text INI [weapon reorder] section

### C2: Kotlin interface
- NativeAutoselectPatcher.kt with JNI declarations

### C3: Compose UI
- AutoselectEditorPage.kt: D1/D2 toggle, two drag-to-reorder lists
- Separator "--- Never Autoselect below ---" is draggable
- Long press + drag reorder. Save button on changes
- D1 Quad Laser (index 16) displayed as "Quad Lasers", positionable like any item

### C4: Navigation
- Button in SetupActivity, showAutoselectPage state flag

### Design decisions
- Single sensitivity slider for mouse mode (controls both conversion and cap)
- D1 Quad Laser: keep as separate ordering item matching in-game menu
- New pilots: get game default ordering, no launcher interference
- Read from first pilot, write to all pilots (matches controls editor pattern)
- Hardcoded English weapon names (engine text system needs game data loaded)

### Weapon reference
- D1 Primary: Laser(0), Vulcan(1), Spreadfire(2), Plasma(3), Fusion(4), Quad(16)
- D1 Secondary: Concussion(0), Homing(1), Proximity(2), Smart(3), Mega(4)
- D2 Primary: Laser(0)..Fusion(4), Super Laser(5), Gauss(6), Helix(7), Phoenix(8), Omega(9)
- D2 Secondary: Concussion(0)..Mega(4), Flash(5), Guided(6), Smart Mine(7), Mercury(8), Earthshaker(9)
- Separator: 255 in ordering array
