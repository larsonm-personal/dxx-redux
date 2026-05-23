# Coop indicator clipping fix + controls diagnostic (2025-04-10 session 2)

## Issues addressed

### 1. Sphere clipping first segment snapping -- FIXED
**Root cause**: The clip point was computed as `sphere_center + R * normalize(segment_dir)`,
which only gives the correct on-segment point when the segment passes through the sphere center.
For segments where neither endpoint is the center, the clip point was projected to a random
position on the sphere surface, causing a visible "snap."

**Fix**: Use linear distance interpolation along the actual line segment:
```
t = (keepout_r - prev_dist) / (cur_dist - prev_dist)
clip = prev_point + t * (cur_point - prev_point)
```
This always places the clip point on the actual segment.

### 2. Stuck controls after in-game coop load -- DIAGNOSTIC ADDED
The previous fly_init + PF flags fix did not resolve the issue. Added:
- HUD message "Coop restore: controls reinit" visible on screen (confirms code runs)
- Explicit Player_is_dead = 0 reset
- Per-frame diagnostic logging for 30 frames after restore (via coop_indicator_lines.c):
  control_type, movement_type, physics_flags, Player_is_dead, velocity, thrust
- control_type, movement_type, physics_flags added to introspection JSON (position block)

### 3. Coop save not restoring at game start -- INSTRUCTIONS
No special launcher-advanced log settings needed. The coop_save diagnostic messages
use con_printf which goes to:
- debug log files (when Game Logs category enabled)
- logcat tag "DXX" (INFO level)
- introspection ring buffer (last 50 lines)

## How to check logs after testing

### Controls issue (stuck after in-game load)
1. Do the in-game coop load
2. Note if "Coop restore: controls reinit" appears on screen
3. Run: `./android/introspect.sh console` -- shows last 50 con_printf lines
4. Run: `./android/introspect.sh` -- position block now includes control_type/movement_type/physics_flags
5. Check debug logs: enable "Game Logs" in Advanced Settings, then export from launcher

### Coop save at game start
1. Start a coop game with a save file selected
2. After it fails to restore, run:
   - `./android/introspect.sh console`
3. Look for lines starting with `coop_save:` which trace the auto-restore flow

## Files modified
- android/app/src/main/cpp/shared/coop_indicator_lines.c -- sphere clipping fix + per-frame diagnostic
- android/app/src/main/cpp/shared/coop_indicator_lines.h -- added coop_indicator_diag_trigger()
- android/app/src/main/cpp/shared/game_introspect.cpp -- added control_type/movement_type/physics_flags
- d2/main/state.c -- HUD msg, Player_is_dead reset, expanded logging, diag trigger
- d1/main/state.c -- same as d2
