# Level 8 replay analysis 2026-05-09 173157

## Goal
- determine why the new D2 level 8 demo desyncs
- identify whether the one-powerup final mismatch comes from a missed pickup, an object lifecycle divergence, or an earlier upstream state split

## Steps
- [done] locate the recorded demo, rng trace, expected result, and replay sandbox output for timestamp 20260509_173157
- [done] rerun the same demo with focused state and RNG tracing so the first mismatch frame is visible
- [done] inspect the preserved replay sandbox logs and result traces to find the earliest powerup-related divergence
- [done] compare recorded and replay rng traces to find the first behavioral random-call mismatch
- [done] mirror upstream awareness, chase-path, and follow-path owner logs into the durable replay probe output and rerun around the first rng mismatch
- [in_progress] compare recorded vs replay object 176 visibility and path lifecycle between frames 901 and 909 to explain why replay kills the follow path at frame 903 and then misses the later recorded path rebuild at frame 909

## Notes
- user reported replay failure in temp\input_demo_runtime_wrapper\d2\d2_descent2_level8_20260509_173157\results\result.actual.json with final mismatch result.level_summary.powerups_remaining expected 99 vs actual 100
- frame 459 is a real shared removal, not a replay miss: both recorded and replay traces drop powerups_remaining from 100 to 99 there when obj 114 sig 14008 id 37 seg 38 is removed
- frame 929 is the first real replay-only mismatch: replay powerups_remaining changes from 99 to 100 while RNG state and player pose still match the recorded trace
- the new generic powerup live-delta probe shows replay adds obj 186 sig 14608 id 22 seg 191 at frame 929; id 22 is POW_VULCAN_AMMO
- the source-object probe shows the spawned ammo comes from object_create_egg() in fireball.c for source obj 83 sig 14423 type OBJ_ROBOT id 36 seg 191 with flags 0x1 (OF_EXPLODING)
- this means the current desync is not pickup bookkeeping; replay runs the exploding-robot egg path and spawns a new Vulcan ammo box that the recording never counted
- rng trace compare gives the earlier upstream split: comparable line 5631 expects `d2/main/aipath.c:create_random_xlate` at frame 909 for ctx obj 176 sig 14434 id 30, while replay instead reaches `d2/main/ai.c:create_awareness_event` at frame 910
- the replay-only second awareness call is now attributed: frame 910 is `source=collide_weapon_wall source_obj=177 aux_obj=36`, while frame 909 is the shared `collide_weapon_robot` awareness from source obj 179 aux obj 83
- robot 176 already takes one shared lost-visibility chase gate in replay at frame 901: `AIM_CHASE_OBJECT` with `prev_vis=2 player_vis=0` creates a fresh path and changes mode `3 -> 2` with path length `11 -> 9`
- replay then kills that new path at frame 903 in the owning `AIM_FOLLOW_PATH` logic, not inside `ai_follow_path()`: `follow_path_transition` shows `still_pass=1`, `mode=0`, and `path_length=0`
- by frames 904 through 909 replay has robot 176 back in chase mode with `prev_vis=0 player_vis=0 path=0/0`, so the frame 909 direct chase-path gate cannot fire there; the first rng mismatch now looks like a recorded-only second path rebuild, not the first path build itself
- the next owner-level question is which non-rng state differs on the recorded side so that robot 176 does not take the frame 903 `AIM_FOLLOW_PATH` still gate, or later regains `previous_visibility == 2` before frame 909