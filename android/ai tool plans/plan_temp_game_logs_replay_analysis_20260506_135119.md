# Plan: Temp Game Logs Replay Analysis 2026-05-06 13:51:19

Goal:
- identify the earliest reproducible replay desync for `android/temp_game_logs/d2_descent2_level3_20260506_135119.dximdemo`
- determine whether the visible thief-player contact near frame 2800 is causal or later fallout
- pin the desync to the narrowest concrete subsystem or event path practical from host replay evidence

Plan:
- [x] run the standard D2 host replay with state-trace and RNG-trace compare enabled for the demo
- [x] inspect the first mismatch frame and the earliest RNG divergence, if any
- [x] inspect sandbox log evidence around the mismatch window, with special attention to thief, collision, and awareness paths
- [x] decide whether the thief contact is the root cause, a secondary consequence, or unrelated
- [x] move the replay RNG compare/restore point to the post-control boundary and validate against the failing demo
- [x] isolate the next remaining causal split after the frame-2671 fix
- [x] rule out replay-control payload mismatch as the active cause around frames 2814-2816
- [x] pin the remaining split to hidden robot-95 AI/path state drift rather than homing target acquisition
- [x] capture the earliest replay-only robot-95 hide/path/cooldown drift before the later replay-only weapon collision and delayed homing hit
- [x] identify which earlier robot-95 AI/path update first moves replay onto the too-early combat path that leads to the replay-only fires at 2775 and 2779
- [x] add recording-side agitation-path instrumentation so future demos can capture the same branch without relying on replay-only logs
- [ ] determine whether the replay-only agitation-path gate hits at frames 2592, 2611, and later are caused by earlier RNG drift or by mismatched gate inputs before the rolls

Status:
- investigation started from the existing `android/tests/run_input_demo_replay.ps1` host replay wrapper and the centralized replay mismatch logging in `d2/main/input_demo_hooks.c`
- the later score and extra robot kill around frame 2815 are confirmed fallout, not the first cause
- the first causal split is the hidden frame-2671 awareness RNG use tied to a replayed secondary-fire path
- recording captures frame RNG after input-event-driven `do_weapon_n_item_stuff()` runs, while replay was restoring that post-control RNG before `ReadControlsReplayFrame()` re-ran the same control side effects
- replay now restores recorded RNG after `ReadControlsReplayFrame()` and before `GameProcessFrame()`, and that fixes the frame-2671 split
- the next earliest visible mismatch remains frame 2816, but the remaining hidden split starts earlier than the replay-only frame-2795 weapon collision and the replay-only frame-2814 AI burst
- the current replay still matches recorded player-facing snapshot state through frame 2815, so the active defect is in hidden runtime state rather than the traced result snapshot
- replay log evidence shows the first concrete hidden divergence earlier than the visible mismatch:
	- from replay frames `2500-2591`, robot `95` is still idle in seg `11` with `hide=-1`, `path_length=0`, and `next_fire=-524288`
	- at replay frame `2592`, robot `95` first takes the replay-only AI/path branch into `mode=2`, `goal_seg=72`, `path_length=9`, `hide=412`, with a large RNG call burst (`calls=20673->20745`)
	- replay advances the hidden path state through `hide=543` at frame `2691`, `hide=551` at frame `2696`, and `hide=576` at frame `2711`
	- replay reaches combat movement first, entering `mode=3` at frame `2759` and seg `92` at frame `2762`
	- replay then fires robot `95` shots at frames `2775` and `2779`, setting `next_fire` to `8192` and then `196608`; the recording's first robot-95 fires are later at frames `2792` and `2796`
	- the recorded robot-95 fire state is already different when it first appears: recording frame `2792` has `hide=586` with `next_fire=-524288`, while replay frame `2792` is still on `hide=576` with `next_fire=162529`
	- by replay frame `2790`, robot `95` is already on the wrong hidden path and cooldown state (`hide=576`, `next_fire=167772`), while the recording reaches frame `2792` with `hide=586` and `next_fire=-524288`
	- at replay frame `2795`, weapon `191` collides with robot `95`; the recorded frame `2795` has no matching `robot_damage` or weapon-robot collision for robot `95`
	- at replay frame `2814`, obj95 takes a large AI/path recompute burst (`calls=24911->24969`, `mode=2`, `goal_seg=82`, `path_length=3`, `hide=665`), while the recorded frame `2814` instead shows the homing missile hitting robot `95`
- homing target acquisition is no longer the leading hypothesis: both recording and replay keep the player homing missile locked on robot `95`; the active problem is that robot `95` reaches the wrong hidden state and pose before the collision window
- `d_tick_*` is restored from the replay checkpoint save path, so the strongest current hypothesis is no longer a missing checkpoint restore; the closer next hop is the earlier robot-95 AI/path update that advances replay onto the combat path about 17 frames before the recording
- the direct owner for that earlier update is the pre-switch agitation branch in `d2/main/ai.c` that randomly calls `create_path_to_player()` for non-still robots when `Overall_agitation > 70` and `dist_to_player < F1_0*200`
- replay frame `2592` confirms the first wrong branch hit for robot `95` while it is still `AIB_BEHIND` / `AIM_BEHIND`: the new probe shows `trigger_roll=285 < FrameTime/4` and `path_roll=26134`, which passes the second gate and immediately feeds `create_path_to_player()`; the paired `ai_rng` line then shows `goal_seg=72`, `path_length=9`, `hide=412`, and `seen=7025459`
- the same agitation gate re-fires at replay frame `2611` (`trigger_roll=67`, `path_roll=27214`) to retarget robot `95` from `goal_seg=72` to `goal_seg=79`, and again at frame `2623` (`trigger_roll=81`, `path_roll=27262`) to refresh the path and move `hide` from `430` to `444`
- later successful gate hits at replay frames `2691`, `2696`, `2711`, and `2814` keep rebuilding robot `95` onto the too-early combat path that later promotes to `mode=3` at frame `2759` and fires at `2775`/`2779`
- future recordings now write structured `probe_ai_agitation_path_gate` events directly into the `.dximdemo` file whenever the agitation gate's first roll passes during recording; each event includes the robot identity, behavior/mode, pre/post goal/path state, trigger and path rolls, and seen-time change so the same branch can be checked without reproducing the exact original demo
- the remaining unknown is no longer which branch fires, but why replay gets those successful agitation-path rolls while the recording does not: that next hop needs either recording-side gate visibility or a tighter compare of RNG/gate inputs immediately before frame `2592`