# temp game logs homing desync -- 2026-05-05

Status: short-demo first replay-only extra RNG call isolated to a specific snipe robot; fresh version-4 recording `183034` is now under compare to confirm whether it diverges on the same awareness path or a different first mismatch

Goal:
- analyze the new fresh demo in `android/temp_game_logs`, with special focus on the apparent desync after a robot-fired homing missile hits the player

Plan:
- [x] identify the newest `.dximdemo` artifact and matching sidecars in `android/temp_game_logs`
- [x] replay the demo with result, RNG trace, and debug/probe outputs captured under `temp/`
- [x] inspect embedded recorder probe events around robot homing fire, homing path, player hit, and first replay mismatch
- [x] compare recorded and replay RNG/probe timing around the first trustworthy divergence
- [x] trace the relevant homing missile, collision, and player-damage code paths for a root-cause hypothesis
- [x] add focused player/object/weapon collision probes for frame 100 and rerun
- [x] add a trigger/wall-change probe and rerun to test whether wall 135 is missed because of trigger gating
- [x] confirm wall-135 texel checks are physics FQ_TRANSPOINT calls (not homing visibility FQ_TRANSWALL)
- [x] run A/B replay with debug-only suppression of homing player bump response
- [x] compare frame-98 wall-135 texel samples and sig=189/190 outcomes between A/B runs
- [x] capture pre-check `fvi_side2544_call` rays for frame-98 blocked wall tests (obj 38/39)
- [x] enable homing desync probe logging during input-demo recording (not replay-only)
- [x] widen the replay probe window enough to capture the full short level-4 control/fail demos and compare the short failing run against the short passing controls
- [x] confirm the short failing demo diverges before the old wall-135 sequence and isolate the earliest short-demo frame-boundary slip
- [x] confirm live recordings currently omit per-frame `state` because recorder `record_per_frame_state` is never enabled in D1 or D2
- [x] confirm the Android `demo_record_per_frame_state` launcher toggle is currently unwired to native recording
- [x] restore D2 replay per-frame state-trace writes and validate with a focused replay trace run
- [x] make expected-state export fail clearly when a demo contains no recorded per-frame `state`
- [x] wire live recorder per-frame state capture for debug replay analysis
- [x] add a narrow frame-boundary probe around frames 129-132 if replay state tracing still leaves the slip ambiguous
- [x] add AI-object context to RNG traces and rerun the failing short replay to identify the first replay-only robot
- [x] capture the same awareness-gate/object context on a fresh recording if player-position lead into the gate remains ambiguous
- [x] replay fresh version-4 demo `d2_descent2_level4_20260505_183034.dximdemo` with actual state and RNG traces captured under `temp/`
- [x] isolate the first fresh-demo mismatch and compare it against the short-demo `obj=98` awareness-slip path
- [x] verify the Android emulator replay handoff for `183034` and capture the on-device failure mode

Notes:
- prefer embedded frame events from the new recorder instrumentation over screenshots or visual inspection
- do not assume the visible homing hit is the first state divergence until the replay mismatch and event timeline agree
- current artifact set: `d2_descent2_level4_20260505_075528.dximdemo`, `.dximdemo.rngtrace.jsonl`, and `.dem`
- this demo is schema version 3 and does not contain the newer embedded recorder event probes
- headless-console and windowed-no-present both reproduce the same final mismatch, so the failure is not specific to the headless runner
- the first hard gameplay RNG divergence is frame 100 / gt 477102, before the visually observed homing missile symptom
- frame 100 recorded RNG order is `create_awareness_event`, `phys_apply_rot`, then `do_ai_frame` and `ai_do_actual_firing_stuff`; replay skips the awareness/rotation pair and uses those same per-frame RNG values for AI instead
- no robot homing weapons are present in the replay probe window around frames 96-103
- current evidence points at a missing player/robot or player weapon/robot collision branch, not a robot homing missile as the first cause
- classic `.dem` JSON dumping has been repaired enough for spatial comparison; output is `temp/d2_l4_075528_classic.jsonl`
- replay trigger probes in frames 0-110 only see no-wall/no-trigger exits, with no `skip_not_connected`, no `change_link_before/after`, and normal `game_mode=0x0`
- wall 135 is `WALL_CLOSED` / `WALL_DOOR_CLOSED` with flags `WALL_BUDDY_PROOF`; replay laser pairs hit it at frames 83, 89, 94, 98, and later for other shots
- the earlier pairs that hit wall 135 are not all divergent by themselves; structured classic parsing shows the important pair is the one aligned with classic `sig=190/191`, where classic crosses from segment 254 into 168 and one shot disappears near robot `sig=31`, while replay `sig=189/190` bounces off wall 135 instead
- the current lead is a wall state or wall-visibility mismatch before the frame-100 RNG branch, not robot homing or multiplayer trigger gating
- all wall-135 texel probes currently report `fvi_flags=0x5` (`FQ_CHECK_OBJS | FQ_TRANSPOINT`), which points to weapon movement checks, not homing visibility rays
- A/B replay with `DXX_INPUT_DEMO_DISABLE_HOMING_PLAYER_BUMP=1` confirms the homing-player bump skip executes at frame 84, but frame-98 wall-135 texel samples (`u=19980` and `u=30394`, `pixel=100`) and `sig=189/190` wall-bounce outcomes remain unchanged
- new pre-check FVI probe confirms frame-98 blocked tests are direct rays from shot objects (`thisobj=38/39`) with `p0=(317614,39712,16027056) -> p1=(334642,44337,15719791)` and `p0=(24653,39329,16009483) -> p1=(41681,43954,15702218)`; these produce hit points exactly matching the blocked texel samples
- recorder support update: `input_demo_append_replay_probe_message` now also emits `{"kind":"probe_log",...}` frame events while recording, and FVI/physics homing-desync probes now use a shared record-or-replay gate so the same wall/ray diagnostics are captured in fresh `.dximdemo` recordings
- build validation: `buildd2` targets `dxx-redux-d2-headless` and `dxx-redux-d2` compile successfully via direct `cmake --build`; helper script `run-windows-build.ps1` hit a local vcvarsall arch-resolution issue (`x86` unsupported in current shell env)
- replay probe window is now widened from frame 110 to 260 for D2 replays so short level-4 demos capture later grate interactions without waiting for the older 437..470 window
- short passing control `d2_descent2_level4_20260505_155604.dximdemo` does reach the old grate sequence under the widened gate: later player shots appear at frames 157, 163, and 169, and wall 135 pass-through is logged at frame 162 for `sig=183/184`
- short failing demo `d2_descent2_level4_20260505_155451.dximdemo` does not reach that late wall-135 sequence at all under the widened gate: there are no later `player_shot_create` or `wall=135` probe lines, and the run is instead still in segment 15 while robot 30 fires a second homing missile (`sig=185`) at frame 178
- the short failing demo also has no `physics_fvi_fate obj=0 fate=object ... hit_sig=179` player-vs-homing collision, while the short passing control does at frame 131; this again argues against the homing-hit bump being the first cause for the short failure
- current short-demo conclusion: the failing short artifact diverges onto a different route before the old transparent-wall / wall-135 event, so it should not be treated as another instance of the original frame-98 grate bounce without additional evidence
- raw short-demo inspection confirms `d2_descent2_level4_20260505_155451.dximdemo` frame records contain `input` and `rng` but no per-frame `state` or `diag`, so `export_input_demo_state_trace.ps1` currently writes a meta-only expected trace for that artifact
- recorder root cause: shared recorder only serializes frame `state` when `record_per_frame_state` is enabled, and live `input_demo_prepare_recorder_settings(...)` in both D1 and D2 never set that flag
- replay root cause: D2 `input_demo_log_current_replay_frame_state_mismatch()` only calls debug stubs for replay state tracing, while D1 has a real `input_demo_state_trace_write_frame(...)` path, so D2 replay-side `.actual_state.jsonl` emission is incomplete
- RNG trace interpretation update: replay restores recorded RNG state at each frame boundary, so the repeated `calls=710->711 state=1358259914->134905915` transition across adjacent failing frames is evidence of a frame-boundary slip, not proof of cumulative extra RNG drift
- D2 replay-side state trace emission has now been restored and validated, and expected-state export now fails clearly on demos that only contain meta/input/rng records
- Android launcher/native recorder wiring now enables live per-frame state capture for fresh debug recordings when the debug toggle is on
- the first replay-only RNG call in `d2_descent2_level4_20260505_155451` is now concretely identified: frame 130 / gt 414384 / `obj=98 sig=99 id=37` in `d2/main/ai.c:645`
- that call site is the proximity-awareness random check, not firing logic: `((obj_ref & 3) == 0) && !previous_visibility && (dist_to_player < F1_0*100)` before `rval = d_rand()`
- the culprit robot is `behavior=132` (`AIB_SNIPE`), so it bypasses the normal timeslice return; the extra RNG is not caused by the timeslice gate itself
- frame 131 replay still uses the recorded `calls=710->711 state=1358259914->134905915` transition on `obj=71 sig=72 id=37`; because the recorded trace has no object context, that proves frame 130 `obj=98` is extra relative to recorded frame 130, but not which robot owns the recorded frame-131 call
- replay `obj=98` is already inside the `F1_0*100` awareness radius on frame 129 (`dist=6474773 < 6553600`) with `obj_ref=31`, then fires on frame 130 when `obj_ref=28` makes `((obj_ref & 3) == 0)` true; the remaining unknown is what earlier replay player/world-state lead put robot 98 inside that radius by frame 129
- fresh artifact set now also includes `d2_descent2_level4_20260505_183034.dximdemo`, `.dximdemo.rngtrace.jsonl`, and `.dem`
- `183034` is schema version 4 and already embeds per-frame `state` plus recorder-side probe events, so fresh replay analysis can compare recorded and actual state directly instead of relying on a meta-only export
- recorded `183034` RNG sidecar now includes `ctx_obj` / `ctx_sig` / `ctx_id`; the early recorder-side AI RNG sequence does not match the earlier short failing `obj=98` frame-130 pattern, so the new run needs an actual replay compare before assuming the same root cause
- fresh-demo conclusion: `183034` does not first diverge on the old short-demo `obj=98` awareness path; the first concrete gameplay split is frame 109 / gt 553190, where the replay-correlated player shot reaches seg 254 side 4 and is classified as blocked by a transparent wall texel sample
- the fresh-demo wall branch is `physics.c` weapon movement using `FQ_TRANSPOINT`, then `fvi.c::check_trans_wall()` sampling `bm->bm_data` from `GameBitmaps[...]` or software `texmerge_get_cached_bitmap(...)`; this path does not consult OGL `gltexture` PNG/KTX replacement alpha
- `wall.c::check_transparency()` still uses stock pig transparency flags via `piggy_bitmap_get_flags(...)` to decide whether the side is a transparent portal at all; for the failing frame the side remains `wid=0x6`, but the sampled texel itself is opaque (`pixel=16`, `pass=0`) in replay
- consequence for the user's 512 texture pack suspicion: a hi-res replacement can render an alpha cutout that looks passable while gameplay still uses the stock palette bitmap or stock texmerge result for shot pass-through, so a stock-vs-hires mask mismatch remains a live root-cause candidate
- Android emulator follow-up is no longer blocked on device availability: `SetupActivity` receives the replay extra and the game process logs `Launching input demo replay: /data/user/0/com.dxxredux.app/files/replay_verify/verify_183034.dximdemo`
- current Android blocker is earlier than replay desync comparison: replay startup aborts during checkpoint restore in `state_restore_all_sub()` with `calc_controlcen_gun_point()` asserting `obj->type == OBJ_CNTRLCEN` at `d2/main/cntrlcen.c:71`, so no `.actual.json` is written yet