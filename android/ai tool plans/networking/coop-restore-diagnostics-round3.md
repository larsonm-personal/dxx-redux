# Coop Restore Diagnostics Round 3

## Findings from log analysis

### Auto-restore never triggers
- `multi_prep_level` sets `PKilledFlags[i]=1` for ALL players at level start
- `multi_send_reappear` clears it to 0, but only after player spawns
- In `coop_try_auto_restore`: timeout check (frames>150) is AFTER the alive check
- If `multi_all_players_alive()` returns false, function returns early every frame
- The timeout at 150 frames is UNREACHABLE -- the auto-restore loops forever
- Fix: reorder timeout before alive check, add per-player PKilledFlags logging

### Stuck controls
- Engine state is correct: ct=4 mt=1 pf=0x4b dead=0 (all 30 diag frames)
- pf=0x4b = PF_TURNROLL(0x01)|PF_LEVELLING(0x02)|PF_WIGGLE(0x08)|PF_USES_THRUST(0x40) -- correct
- Velocity and thrust are 0,0,0 for all 30 frames after restore
- Need to log Controls struct (forward_thrust_time, pitch_time, etc.) to determine
  if the problem is touch input not generating events, or game loop not applying them
- Also need to check window state (is Game_wind the front window?)

### P0 not mapped during restore
- Host (Player32) was not found in save file's player list
- Likely because save was from a different session with different callsigns
- Need to add diagnostic logging for failed callsign comparisons

### Indicator lines snapping
- Path waypoint 0 is anchored to player pos at UPDATE time (every 30 frames)
- Between updates, player moves, making segs[0] stale
- Distance check at render time uses current ConsoleObject->pos
- Stale segs[0] drifts out of keepout sphere, causing lines to pop in behind player
- Fix: re-anchor segs[0] to current player position at render time

## Changes

### 1. coop_indicator_lines.c
- [x] Re-anchor segs[0].point = ConsoleObject->pos before draw_path_lines calls
- [x] Add Controls struct fields to diag output (forward_thrust_time, pitch_time, etc.)
- [x] Add window state check (Game_wind == front window)

### 2. coop_save.c (d2 only, d1 has no auto-restore)
- [x] Reorder timeout check before alive check in coop_try_auto_restore
- [x] Add COOPLOG at each goto-disarm with reason
- [x] Log per-player PKilledFlags+connected when alive check fails

### 3. state.c (d2 and d1)
- [x] Add logging for failed callsign comparisons in coop mapping loop
- [x] Log saved player callsigns/connected at start of mapping

### 4. Build and verify
- [x] Build clean
