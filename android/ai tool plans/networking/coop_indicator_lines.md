# Coop Indicator Lines - Path Lines to Guidebot and Nearest Player

## Overview
Draw faint 3D lines along segment paths to the guidebot (D2 only, light blue) and nearest
player (light green) during coop gameplay. When target is behind the player, render a 3D
arrow pointing rearward with a label ("guidebot" or "[callsign]").

## Architecture

### New files
- `android/app/src/main/cpp/shared/coop_indicator_lines.c` - all path/render logic
- `android/app/src/main/cpp/shared/coop_indicator_lines.h` - public API (2 functions)

### d1/d2 edits (minimal)
- `d1/main/render.c` / `d2/main/render.c`: one `#ifdef __ANDROID__` hook in render_frame()
- `d1/main/multi.h` / `d2/main/multi.h`: add `NETGAME_FLAG_COOP_QOL 128`
- `d1/main/net_udp.c` / `d2/main/net_udp.c`: add checkbox in "more options" menu

### CMakeLists.txt
- Add `shared/coop_indicator_lines.c` to both D1 and D2 target_sources blocks

## Design Details

### Path computation (coop_indicator_lines_update)
- Called once per frame from render_frame, before the draw call
- Recomputes paths every ~30 frames (0.5s at 60fps) using frame counter
- Uses `create_path_points(ConsoleObject, my_seg, target_seg, local_buf, ...)` 
- max_depth=20 to cap BFS cost  
- Guidebot: read live path from Point_segs[aip->hide_index] if available, else compute
- Player: find nearest connected coop player by distance, compute path

### Line rendering (coop_indicator_lines_render)
- Called from render_frame() between render_mine() and g3_end_frame()
- Filter path segments: only draw line for point_segs whose segnum appears in Render_list
  (gives free wall occlusion -- lines disappear around corners)
- Colors: BM_XRGB(10,10,31) blue for guidebot, BM_XRGB(10,31,10) green for player
- Transparency: gr_settransblend(18, GR_BLEND_ADDITIVE_A) -- faint glow
- For each consecutive pair in path: g3_rotate_point + g3_draw_line
- Distance cap: skip rendering if path_length > 25 segments

### Rearward 3D arrow
- When target point has CC_BEHIND after g3_rotate_point, draw arrow instead of lines
- Arrow placed in world space, offset from player position in rearward direction
- Compute direction from player to target, project onto player's view plane
- Place arrow behind+above player: pos + uvec*size - fvec*size + dir_component*size
- Draw as 3 lines (shaft + two arrowhead wings) like automap draw_player pattern
- Label rendered with gr_string at projected screen coords of arrow base

### Server-wide toggle
- NETGAME_FLAG_COOP_QOL = 128 (bit 7 of game_flags ubyte)
- Controls: guidebot spawning, helper arrows, buddy health HUD, warp-to-player
- Added as checkbox in net_udp_more_options() menu: "Coop QoL Enhancements"
- Default: on (flag set by default in netgame_set_defaults or setup)
- Preserved through host migration (Netgame struct is not reset)
- No save file storage needed -- set per server session

## Status
- [x] Research complete
- [x] Phase 1: NETGAME_FLAG_COOP_QOL flag + menu toggle
- [x] Phase 2: coop_indicator_lines.c/h implementation
- [x] Phase 3: render.c hooks
- [x] Phase 4: CMakeLists.txt
- [x] Phase 5: Build + fix (zero warnings)
- [ ] Phase 6: Test on emulator (coop multiplayer needed)
