# Coop indicator lines + save/load bugs

## Issue 1: Sphere clipping -- FIXED
The keep-out sphere was deleting entire line segments if either endpoint was inside the
sphere, rather than clipping the line at the sphere boundary. Fixed to handle all four
cases: both outside (draw full), crossing out (clip start), crossing into (clip end),
both inside (skip). Clip points are computed along the segment direction at keepout_r
from the player center.

File: `android/app/src/main/cpp/shared/coop_indicator_lines.c`

## Issue 2: Coop save not restoring at game start -- LOGGING ADDED
Probable cause unknown from code reading alone. The flow is:
1. Kotlin launcher writes `coop_restore_slot.txt` with save slot number
2. C engine reads this file in `coop_arm_auto_restore()` (called from `multi_do_frame`)
3. Arms the restore and triggers it after 30-150 frames when all players are connected

Possible failure points:
- `coop_restore_slot.txt` not written or not visible to PhysFS
- `state_get_game_id()` returns 0 (save file missing or corrupt)
- `coop_auto_restore_attempted` stuck from a previous game session (unlikely, reset in
  `coop_disarm_auto_restore()` called from `multi_new_game()`)
- `multi_all_players_alive()` never returns true within the 150-frame window

Logging added at: rejection points in `coop_arm_auto_restore()` (not-coop, not-master),
file name and game_id for both callsign and autosave filenames.

To diagnose: capture console output (introspect.sh console) right after starting a coop
game with a save selected. Look for "coop_save:" prefixed log lines.

## Issue 3: Stuck controls after in-game coop load -- LIKELY FIXED + LOGGING
Symptom: Both players at saved positions but can't translate, can only slowly aim.

Root cause analysis: After `state_restore_all_sub()` runs the coop section, the console
player's object goes through `multi_reset_player_object()` which clears PF_TURNROLL,
PF_LEVELLING, PF_WIGGLE from the physics flags. The function does NOT set CT_FLYING or
PF_USES_THRUST. While the saved object properties are copied beforehand (including
control_type from the save), `fly_init(ConsoleObject)` is never called after the coop
restore section. In the normal game startup path, `fly_init` is called from `game_init()`
when the game window is created, but during a restore the window already exists.

If the saved control_type was CT_NONE (possible for remote player objects in a shared
autosave), the console player's object would have CT_NONE after restore, which means
`read_flying_controls()` is never called (line ~1945 in object.c: `case CT_NONE: break;`).
This would cause no rotation or translation at all.

Even with CT_FLYING from a per-player save, missing PF_USES_THRUST would prevent thrust
from translating into acceleration (physics.c line 230: thrust only applied when flag set).

Fix applied: Added `fly_init(ConsoleObject)` + explicit PF flag restore after the coop
restore section in both d1/main/state.c and d2/main/state.c. This guarantees CT_FLYING,
MT_PHYSICS, and all physics flags regardless of save contents.

Diagnostic logging added:
- Each player slot mapping during coop restore (callsign, save slot, objnum)
- Post-restore object state (control_type, movement_type, physics flags) per player
- Post-fly_init console object state

Files changed:
- d2/main/state.c (fix + logging)
- d1/main/state.c (fix + logging)
- d2/main/coop_save.c (logging for issue 2)
- android/app/src/main/cpp/shared/coop_indicator_lines.c (sphere clipping fix)
