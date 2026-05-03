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
