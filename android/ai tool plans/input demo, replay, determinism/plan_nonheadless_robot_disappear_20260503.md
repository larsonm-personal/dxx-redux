# Plan: Investigate non-headless robot disappearance near doors (2026-05-03)

## Goal
Determine why robots near door transitions disappear visually during non-headless replay while replay pass/fail remains correct

## Steps
- [x] Identify newest replay artifact and verify available logs
- [x] Add focused render-path probes for tracked robots (list membership + draw skip reasons)
- [x] Build host d2 and run replay wrapper/windowed replay output capture
- [x] Correlate disappearing frames with render gates and determine root cause
- [x] Add object lifecycle probes for delete sweep/object delete/relink/segment update failures
- [x] Add non-render visibility-gate probes (LOS/FVI/wall doorway metadata)
- [x] Re-run exact replay and extract view_gate drop/restore transitions
- [x] Implement minimal wrapper fix for visual vs fast replay selection
- [x] Verify updated wrapper behavior with a reliable replay run
- [ ] Isolate engine-side cause for robots 73/74/75 missing from render traversal
- [x] Isolate late-frame matcen invisibility for robot 158 around frames 1917-1930
- [x] Update this plan with outcomes and evidence

## Findings
- Newest replay artifact: d2_descent2_level2_20260503_091509.dximdemo
- Reliable realtime wrapper validation completed with `-KeepSandbox`; replay passed (`RESULT: PASS`, frame_count=1966)
- Replay output shows the blue-key closet trio as missing object slots, not render-culled survivors
- Example pattern at frames around 1959-1965: tracked robots 73/74/75 report `step=missing` with `slot_type=255`, while peers 70/72 in the same area still report `step=pose`
- `slot_type=255` means OBJ_NONE in that object index, so the object was removed from the simulation object table
- This indicates the visible "disappearance" is caused by object deletion/replacement timing, not by object draw suppression in render passes
- Render probe for target robots (73/74/75) shows all three as non-rendered early in replay: `in_render_seg=0`, `in_obj_list=0`, `drawn=0` (e.g. frame 300)
- Robot 75 has persistent segment/portal rejection evidence: `child_reject_behind parent_seg=39 side=5 child_seg=0 robot_obj=75 viewer_seg=45` across multiple frames (588+), while robot 75 state remains `seg=0 pos=(0,-670084,-19572137)`
- New lifecycle logs in object.c show direct delete events for the trio:
	- obj 73: `delete_sweep` + `obj_delete` at frame 594, seg 45, flags `0x3`, exploding=1, should_die=1
	- obj 74: `delete_sweep` + `obj_delete` at frame 791, seg 81, flags `0x3`, exploding=1, should_die=1
	- obj 75: `delete_sweep` + `obj_delete` at frame 1546, seg 39, flags `0x3`, exploding=1, should_die=1
- `update_seg_fail` did not fire for these targets in this run, so there is no direct evidence yet of segment-loss/out-of-bounds correction immediately before deletion
- `obj_relink` does occur for these robots before death (normal segment transitions while alive, and one while exploding), which supports movement changes but not hard out-of-segment failure
- Visibility-gate diagnostics for the exact replay show two distinct invisibility causes:
	- Occlusion at segment-side boundaries: `transition=drop` with `los=0`, `hit_type=1` (wall/side), and `doorway=0x2`; this is a blocked LOS result, not a renderer-only skip
	- Facing-plane cull: `transition=drop` with `los=1`, `hit_type=2` (direct object), but `front_dot<=0`; this is not wall occlusion, the robot moved behind the view direction
- Example occlusion drop/restore sequence from the same run:
	- obj 92 drops at frame 1098 (`los=0`, `hit_seg=304`, `hit_side=2`, `doorway=0x2`) and restores at frame 1434 (`los=1`, `front_dot>0`)
- For the previously discussed blue-key trio (obj 73/74/75), no `step=view_gate` drop was logged before disappearance in this replay; they remained visible (`in_view=1`) until lifecycle deletion points above
- Focused late-frame replay probes for obj 158 (sig 4871) show it is continuously in all render traversals at 1910-1939: `in_render_seg=1`, `in_obj_list=1`, `in_seg_list=1`, `drawn=1`
- New visual-state logs from `draw_polygon_object` show obj 158 is not entering cloaked rendering in that window: `cloak=0`, `cloak_type=0`, `path=default`
- Lighting at 1910-1939 is stable and non-zero (`light.r/g/b` about 199k-211k), ruling out a simple zero-light black-frame cause
- Additional projection probe shows obj 158 is fully on-screen through the suspect window: `probe_codes=0`, `probe_behind=0`, `probe_projected=1`, `probe_p3_codes=0`, with screen coordinates around `(314,154)` at frame 1917 and smoothly moving through 1935
- Id-family probe for robot id 38 shows exactly one rendered candidate in frames 1910-1935: `obj=158 sig=4871`; there is no competing id-38 robot in that window to explain a mistaken visual target
- Combined evidence indicates the 1917-1930 case is not a render traversal skip and not robot cloak fade logic for obj 158; remaining candidates are perception mismatch (different object slot in visual trace) or a downstream draw/present artifact outside object gating
- **NEW: Polygon face probe** (phase: instrument `draw_polygon_model` per-face counts via `g3_poly_faces_considered`/`g3_poly_faces_drawn` globals in interp.c, reset and logged in object.c around the main probe target draw call):
	- Frames 1880-1933: `faces_considered=53, faces_drawn=23-27` every frame - model interpreter working normally, ~half the faces pass backface culling
	- Frame 1934+: `faces_considered=25, faces_drawn=11-12` - LOD switch to simpler model (robot moving away, `simpler_model` depth threshold fires in polyobj.c)
	- No zero-face-drawn frames in the probe window
	- **Conclusion**: `g3_draw_polygon_model` is submitting faces to `g3_draw_tmap`/`g3_draw_poly` normally. The "invisibility" is downstream of the model interpreter - definitively in the GL rasterizer (depth test or blend state)
	- **Most likely cause**: depth occlusion - the robot spawns inside the matcen structure; the matcen segment is visible from the camera (hence `in_render_seg=1`), but the robot's faces are all behind the matcen wall geometry in the depth buffer. This is expected matcen spawn behavior. The robot becomes visible when it exits the matcen chamber around frame 1930+
- **NEW: Robot 107 non-matcen cross-check** (frames 1380-1435, requested focus):
	- Render traversal still marks robot 107 as drawn through the reported disappearance window: `in_render_seg=1`, `in_obj_list=1`, `in_seg_list=1`, `drawn=1` at least 1408-1435
	- Deep object draw probe confirms robot 107 is fully submitted in the same window:
		- `Input demo robot visual state`: `probe_codes=0`, `probe_behind=0`, `probe_projected=1`, stable on-screen `probe_sxy` (e.g. frame 1415 `(282,195)`, frame 1425 `(352,161)`, frame 1430 `(298,165)`)
		- `Input demo robot poly probe`: `model_num=41`, `faces_considered=80`, `faces_drawn=33-38` from 1408 through 1435
	- Robot 107 is in the `tmap_override` draw path throughout this interval (`tmap_override=158`), but override texture diagnostics show `override_bm_flags=0x0` (not a transparent bitmap-flag path)
	- Robot 107 is actually killed at frame 1432 (`shields 524288 -> -65536`, `dead=1`) and transitions to exploding state at frame 1433 (`flags=0x1`, `exploding=1`), so the 1425-1430 visual loss occurs **before** death while draw submission is still healthy
	- **Conclusion**: this non-matcen case matches the matcen case at the critical layer: replay is still issuing model draw calls and faces. The failure is downstream in visibility at raster time (depth/ordering/state interaction), not in object traversal or polygon-model submission
- **Root-cause synthesis after code review**:
	- Windows D2 builds define `OGL_MERGE` (`d2/CMakeLists.txt`), while the replay sandbox config has `ClassicDepth=0`, so the Windows replay uses render.c's 3-pass OGL alpha ordering path: pass 1 transparent level geometry with `glAlphaFunc(GL_GEQUAL, 0.8)`, pass 2 objects, pass 3 transparent geometry with normal alpha
	- In desktop `OGL_MERGE`, `ogl_start_frame()` did **not** enable `GL_ALPHA_TEST` because the startup condition was `#if defined(OGLES) || !defined(OGL_MERGE)`. That makes render.c's pass-1 `glAlphaFunc(GL_GEQUAL, 0.8)` call a no-op on Windows desktop OGL_MERGE
	- With alpha test disabled, transparent/door wall faces can write a full invisible/low-alpha depth surface before objects render. The later object pass then submits robot faces normally, but every pixel behind the invisible depth surface fails depth. This exactly matches both robot probes: render traversal and polygon submission remain healthy, but the robot is visually gone
	- Android is a plausible counterexample because the Android/GLES OGL_MERGE path has separate transparent-wall handling and the startup condition already enables the alpha-test-equivalent path for `OGLES`; it also has Android-only merged-wall/cutout logic around transparent portal faces
	- Minimal trial fix applied in `d2/arch/ogl/ogl.c`: enable `GL_ALPHA_TEST` unconditionally in `ogl_start_frame()` and set the default threshold to `0.02`, so the existing render.c high-alpha prepass thresholds take effect again on Windows desktop OGL_MERGE
	- Build verification: `ninja dxx-redux-d2 dxx-redux-d2-headless` in `buildd2` completed and linked successfully after the fix; warnings were existing `loadgl.h`/GLEW macro redefinitions plus `ogl_get_free_texture` C4715
	- Next hard validation: replay the same demo visually on Windows with the fix. If the robots reappear, this closes the defect. If not, add a one-frame diagnostic that disables depth test only around obj 107/158 draws; that will distinguish invisible depth occlusion from any remaining color/blend/texture state issue

## Replay fidelity direction
- Windowed replay should remain the canonical visual-fidelity path: same game executable, same window/event loop, same render traversal, same gameplay code, with only the control source swapped to recorded input
- Headless and `-inputdemo-norender` should remain explicit fast-path test modes for CI and diagnosis, not the reference answer for what a replay "looks like"
- Engine code already leans this way: `game_setup()` still creates the game window when replay is loaded even if `SysInputDemoNoRender` is set, while the hard draw bypass in `event.c` is specifically gated on `SysInputDemoNoRender && input_demo_replay_is_loaded()`
- The wrapper should make this distinction obvious in naming and defaults so visually inspected replays do not accidentally run through the headless or no-render runners
- Follow-up implementation should focus on one contract:
	- "visual replay" = standard executable + render/present path active
	- "fast replay" = headless or no-render, allowed to diverge in draw/present behavior but expected to match simulation/state outputs
- Any replay-only fixes for the visual path should prefer removing harness/runtime special cases over adding new replay-specific branches in the game renderer

## Wrapper follow-up
- `android/tests/run_input_demo_replay.ps1` now has an explicit `-Runner` contract:
	- `visual` = full game binary + normal render/present path
	- `fast` = explicit fast path, preferring headless when available and otherwise using `-inputdemo-norender`
	- `windowed-no-present` and `headless-console` remain direct explicit fast-path selections
- `auto` now defaults to the visual windowed path unless legacy fast switches are explicitly requested
- Focused validation is still pending for one reliable shell run because terminal capture during the earlier wrapper replay attempts was not trustworthy
