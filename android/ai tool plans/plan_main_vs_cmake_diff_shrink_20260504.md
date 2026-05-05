# main vs cmake diff shrink plan -- 2026-05-04

Status: active

Goal:
- shrink the `d1/` and `d2/` branch diff for `main..cmake` by moving Android-only helper bodies out of legacy game files and into the existing Android/shared or per-game hook surfaces

Baseline survey:
- `git merge-base main cmake` is `fb555eec75e1ed12c8348805ab335afb4c721b06`, matching local `main`
- `origin/HEAD` points to `main`, so `main` is the correct parent baseline for this pass
- `git diff --stat main..cmake -- d1 d2` shows the remaining largest legacy churn clusters in `arch/ogl/ogl.c`, `newdemo.c`, `net_udp.c`, `game.c`, `multi.c`, `state.c`, and the still-active input-demo instrumentation files
- the completed low-risk cleanup tranche moved the remaining collision probe and event helper bodies out of the legacy `d1/main/collide.c` and `d2/main/collide.c` files
- the nearest remaining shrink targets are now the larger `main..cmake` hotspots outside the extracted collision and escort helper surfaces, such as `game.c`, `state.c`, `newdemo.c`, `net_udp.c`, `multi.c`, and `arch/ogl/ogl.c`

Plan:
- [x] survey `main..cmake` branch topology and d1/d2 diff hotspots
- [x] choose the next low-risk extractable cleanup slice from the branch diff
- [x] move the remaining player-bump helper bodies out of `d1/main/collide.c` and `d2/main/collide.c`
- [x] remove stale D2-only collision debug leftovers that the extraction makes redundant
- [x] move the cross-file collision logging hook declarations into `input_demo_hooks.h` and remove the redundant local `extern` blocks
- [x] move the D2 AI and robot-fire hook declarations into `input_demo_hooks.h` and remove the redundant `ai.c` and `ai2.c` local `extern` blocks
- [x] move the remaining low-risk D2 single-function hook declarations such as `gauges.c` score logging into `input_demo_hooks.h`
- [x] move the D2 object lifecycle and robot-visual probe declarations into `input_demo_hooks.h` and remove the redundant `object.c` local `extern` block
- [x] finish the D2 path-trace header cleanup by removing the final `aipath.c` local `extern` without exposing `point_seg` from the shared header
- [x] move the D2 laser input-demo helper block into `input_demo_hooks.c` and remove the remaining static `input_demo_*` helper bodies from `laser.c`
- [x] move the remaining collision-state helper bodies out of `d1/main/collide.c` and `d2/main/collide.c`
- [x] move the remaining low-risk collision probe and frame-event helper bodies out of `d1/main/collide.c` and `d2/main/collide.c`
- [x] move the D2 escort input-demo helper block into `input_demo_hooks.c` and remove the remaining static `input_demo_*` helper bodies from `escort.c`
- [x] validate with D1/D2 host build, focused replay smoke, and Android arm64 native build
- [x] update the long-running shrink and input-demo extraction plans with the completed tranche and any remaining branch-diff candidates

Completed tranche:
- moved `input_demo_log_player_bump_probe(...)` into `d1/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.c`
- removed the now-redundant local player-bump helper bodies from both legacy `collide.c` files and dropped the stale D2-only bump gate
- moved the cross-file collision logging declarations into `d1/main/input_demo_hooks.h` and `d2/main/input_demo_hooks.h`, keeping the public hook surface out of the legacy `collide.c` files
- moved the D2 AI awareness and robot-fire hook declarations into `d2/main/input_demo_hooks.h`, removing the local declaration blocks from `ai.c` and `ai2.c`
- moved the D2 score logging declaration into `d2/main/input_demo_hooks.h`, removing the one-off local declaration from `gauges.c`
- moved the D2 object lifecycle and robot-visual probe declarations into `d2/main/input_demo_hooks.h`, removing the local declaration block from `object.c`
- moved the D2 path-trace declarations into `d2/main/input_demo_hooks.h` and changed `input_demo_log_path_points(...)` to use a read-only opaque buffer so `aipath.c` no longer needs a local declaration
- moved the D2 laser input-demo helper block into `d2/main/input_demo_hooks.c`, leaving `d2/main/laser.c` with direct call sites but no remaining static `input_demo_*` helper bodies
- moved the remaining collision-state helper bodies into `d1/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.c`, leaving no local `input_demo_trace_collision_*` or `input_demo_replay_collision_probe_active()` helpers in the legacy `collide.c` files
- moved the remaining low-risk D1 collision probe helpers and D2 collision frame-event recorder helpers into the per-game `input_demo_hooks.c` files, leaving no local `input_demo_log_player_robot_contact_probe(...)`, `input_demo_log_weapon_robot_accept_seq(...)`, `input_demo_record_*_event(...)`, `input_demo_record_frame_event_json(...)`, or `input_demo_replay_powerup_probe_active()` bodies in the legacy `collide.c` files
- moved the D2 escort probe helpers and their snapshot state into `d2/main/input_demo_hooks.c`, leaving no local static `input_demo_*` helper bodies in `d2/main/escort.c`
- verified there are no remaining local `extern input_demo_*` declarations in `d1/main/*.c` or `d2/main/*.c`
- validated with `run-windows-build.ps1 -Target both`, the focused D2 headless replay smoke, and Android arm64 Gradle tasks `:app:buildCMakeDebug[arm64-v8a]` and `:app:buildCMakeDebug[arm64-v8a]-2`