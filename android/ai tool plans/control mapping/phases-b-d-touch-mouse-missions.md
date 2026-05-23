# Phases B-D: Double-Tap Fix, Exponential Mouse, LAN Mission Picker

## Phase B: Right Stick Double-Tap Fix (Issue 1)

### Problem
HumanReadableConfig.parseStick() doesn't parse mouseMode or mouseSensitivity.
If a config was round-tripped through human-readable export/import, the stick
loses its mouseMode flag -- so it hits the wrong code path on touch-down.

### Fix
Add two lines to parseStick() to read mouseMode and mouseSensitivity from JSON.
The export path (stickToHuman -> toJson) already writes them.

### Files
- android/app/.../HumanReadableConfig.kt: parseStick() ~L162

---

## Phase C: Exponential Scaling for Mouse Mode (Issue 2)

### Problem
Mouse-mode drag uses linear scaling. User wants exponential: the further from
the initial touch-down, the faster the movement. Default 3x at half screen.

### Design
- Track touch origin in StickState (mouseOriginX, mouseOriginY)
- In updateStickFromMouseDrag(), compute distance from origin
- Map distance to multiplier: linear ramp from 1.0 to mouseExponentialMax
- Half-screen distance = screen height/2 as reference

### New fields in AnalogStickControl
- mouseExponential: Boolean = true (enable/disable)
- mouseExponentialMax: Float = 3.0f (max multiplier at half-screen)

### Files
- android/app/.../TouchControl.kt: AnalogStickControl fields + toJson/fromJson
- android/app/.../TouchOverlayView.kt: StickState + updateStickFromMouseDrag
- android/app/.../HumanReadableConfig.kt: parseStick (new fields)

---

## Phase D: LAN Mission Picker (Issue 3)

### Problem
Hosting a LAN or online game required typing a mission filename by hand.
User wanted a picker that shows available missions by display name.

### Implementation (Kotlin-only, no JNI needed)
Chose Kotlin-native parsing over JNI because the engine isn't initialized
during the launcher. The mission file format is trivially simple (name=, type=).
Builtins are hardcoded with a sync note referencing the C headers.

**The existing auto-host path (nativeSetAutoHost -> load_mission_by_name) already
handles mission loading. No C-side changes needed.**

### New file: MissionPicker.kt
- MissionScanner object: scans active file set dir for .msn/.mn2 files,
  parses first few lines for display name and anarchy flag, adds hardcoded
  builtins (d2="Descent 2: Counterstrike!", descent="Descent: First Strike",
  D1 builtin with filename="")
- MissionPickerField composable: read-only text field + picker dialog
- MissionPickerDialog composable: search + LazyColumn of missions

### Modified files
- LanDiscoveryTab.kt: HostLanGameDialog replaced OutlinedTextField with
  MissionPickerField, added missionSelected tracking for enable check
  (D1 builtin has filename="" which is not blank-checkable)
- MultiplayerScreen.kt: CreateLobbyDialog same changes

### Constant sync notes (C <-> Kotlin)
- d2/main/mission.h: FULL_MISSION_FILENAME="d2", D1_MISSION_FILENAME="descent",
  SHAREWARE_MISSION_FILENAME="d2demo", MISSION_DIR="missions/"
- d1/main/mission.h: D1_MISSION_FILENAME="" (empty string for D1 engine)
- Both: mission file format: name=, xname=, zname= for display name;
  type=anarchy for anarchy-only flag

### Status: COMPLETE
