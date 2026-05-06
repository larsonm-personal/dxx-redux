# temp game logs replay analysis -- 2026-05-04 fresh desyncs

Status: analysis in progress, recorder instrumentation added for fresh demo capture

Goal:
- identify the first desync point in the two new demos under `android/temp_game_logs`, compare their RNG and state drift patterns, and narrow the likely controlling code path for each

Plan:
- [x] identify the two fresh `.dximdemo` artifacts and their `.rngtrace` sidecars in `android/temp_game_logs`
- [x] run focused replay checks for both demos and capture the first failing frame or mismatch surface
- [x] compare recorded vs replay RNG traces around the first divergence for both demos
- [x] compare any available per-frame state outputs around the first mismatch to look for shared patterns
- [x] step to the nearest controlling code path for the earliest common or highest-confidence divergence
- [x] summarize findings, likely cause, and the next smallest discriminating instrumentation or fix
- [x] add a durable level 4 AI scheduling probe and rerun the replay at the first bad window
- [ ] capture the missing level 4 `create_awareness_event` and `phys_apply_rot` identities with a durable probe
- [x] reassess homing-projectile and claw-robot hypotheses against the first-failure evidence
- [x] add recorder-side structured probe events for fresh demo capture across projectile, awareness, homing, and claw-contact paths
- [x] build the touched D2 target and confirm the new probe hooks compile cleanly

Notes:
- prefer the first concrete divergence frame over end-of-demo drift
- reuse existing replay/rng trace helpers before adding new instrumentation
- direct headless replay gives the most trustworthy early anchor via `Input demo replay rng state mismatch`, not the raw rng trace line-by-line compare
- the raw rng traces include coverage drift and duplicate entries; compare hits at frame 457 and frame 1316 overstate how early the underlying RNG state diverges
- level 3 first trustworthy frame-state RNG mismatch is frame 1317 / gt 7410744, where replay is one RNG step behind expected and the nearby traced site is `create_awareness_event` in [d2/main/ai.c](d2/main/ai.c#L1403)
- level 4 first trustworthy frame-state RNG mismatch is frame 458 / gt 1758986, where replay is two RNG steps behind expected and the missing expected calls are `phys_apply_rot` in [d2/main/physics.c](d2/main/physics.c#L1494) and a nearby `do_ai_frame` RNG site in [d2/main/ai.c](d2/main/ai.c#L595)
- D2 replay still creates `actual_state.jsonl` meta only; frame-state rows remain unavailable, so the new durable scheduling data is currently coming from a dedicated `ai_schedule_probe.log` beside the replay result
- level 4 actual replay does not hit the `SKIP_AI_COUNT` path in frames 454-460; the visible AI scheduling changes are time-slice returns versus process entries, so the earlier `SKIP_AI_COUNT` hypothesis is no longer the best local fit for this demo
- level 4 actual replay AI `d_rand()` calls in this window come from time-sliced robots before the return gate: object 104 is eligible at frames 456-457, no robot is eligible at frame 458, and object 30 becomes eligible at frame 459
- level 4 recorded versus actual drift is already inside frame 457, not first caused by frame 458 AI work: recorded consumes `create_awareness_event` call 4085, `phys_apply_rot` call 4086, and `do_ai_frame` call 4087, while actual consumes only `do_ai_frame` call 4085 and then stays two calls behind until a later `do_ai_frame` at frame 459
- level 4 actual replay does not enter `create_awareness_event` at all in frames 457-459, and it never reaches the random `phys_apply_rot` branch there either, so the recorded frame 457 awareness and rotation RNG calls are absent wholesale on the actual path rather than merely failing an inner gate
- the remaining level 4 awareness source search space is small: `collide_player_wall`, `collide_weapon_wall`, `collide_player_robot`, `collide_weapon_robot`, `laser_player_fire`, `game_fusion_warmup`, and `ai2_robot_fire`
- actual replay does produce delayed `collide_weapon_robot` plus `phys_apply_rot` pairs against robot 30 later in the same window: `sig 319` at frame 463 and `sig 323` at frame 469, both from player gun-0 laser shots created earlier
- the strongest current level 4 candidate is the missing frame-437 gun-0 shot `sig 315`: it is created at frame 437, stays alive through frames 457-470, never records a wall or robot awareness event in that window, and is still flying in segments 254/15/39 when the recorded trace expected the frame 457 `collide_weapon_robot`/`phys_apply_rot` pair
- the actual gun-0 hit cadence on robot 30 is otherwise consistent in this slice: earlier shots `sig 305`, `sig 308`, and `sig 311` hit at frames 438, 444, and 450, then the missing `sig 315` slot is followed by later hits `sig 319` and `sig 323` at frames 463 and 469
- homing missiles remain a plausible broader replay-risk class because `object_move_all()` advances a global `doHomerFrame`/`homerFrameCount` clock from `FrameTime`, and `Laser_do_weapon_sequence()` uses that gate to retarget and steer homing weapons; the code does checkpoint/restore this state, but any host-vs-recorder frame timing or object-order offset can change when a homer reacquires or turns
- level 4's first-failure path is not locally explained by a robot homing missile: the missing event is a player laser with weapon id 0 (`LASER_ID`), not a homing weapon id, and the recorded/actual RNG window shows no homing-specific random sites
- claw robots are a poor local fit for the level 4 first failure: robot 30/id 51 repeatedly takes the `phys_apply_rot` RNG branch, and that branch is explicitly skipped for `Robot_info[obj->id].attack_type` claw robots in [d2/main/physics.c](d2/main/physics.c#L1512)
- level 3's recorded first window also does not show homing or claw function names in the RNG sidecar, but that is weaker evidence because those paths can alter geometry without directly consuming RNG; level 3 still needs the same durable source/weapon probe style before ruling them out
- current best hypothesis: level 4 is not primarily a `SKIP_AI_COUNT`, claw-robot, or robot-homing-missile bug; it is a normal player laser path/collision divergence around `sig 315`, with later AI RNG drift as fallout
- fresh-demo logging should ride the existing per-frame recorder event stream so the captured `.dximdemo` carries the new evidence without a separate sidecar workflow
- the highest-value missing recorder events are: awareness source plus create_awareness_event transitions, per-frame weapon path/lifetime for player-owned and homing weapons, homing retarget/turn state, `phys_apply_rot` random skip additions, and claw-contact gating/damage context
- fresh-demo recorder coverage now includes structured frame events for: awareness source/entry/gate/result, player or homing weapon lifetime/path, robot fire state with weapon metadata, homing tracking state, `phys_apply_rot` skip additions, and claw-attack gate plus damage steps
- validation: `run-windows-build.ps1 -Target d2` completed successfully after the probe edits, `ctest --test-dir buildd2 -N` reports no configured tests in this build tree, and `android/run-code-quality.ps1 -Fix -Paths @(...)` completed cleanly on the touched scope
- next discriminating check if work resumes: either 1) add a recorder-side version of the current projectile probe and re-record a similar setup to confirm whether the frame-437 equivalent shot hits robot 30 during recording, or 2) keep pushing on replay-only instrumentation around `sig 315` to find the exact path/segment transition around frames 455-456 that lets it miss instead of producing the expected `collide_weapon_robot` event