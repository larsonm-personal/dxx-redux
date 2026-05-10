# Level 7 replay analysis 2026-05-09 140807

## Goal
- determine why the new D2 level 7 demo desyncs
- check whether the widened robot projectile recorder coverage exposes the earlier hidden branch directly
- add general pre-fire gate logging so a fresh demo can capture why robots do not reach the actual-fire branch

## Steps
- [completed] locate the recorded demo, rng trace, and replay artifact directory for timestamp 20260509_140807
- [completed] identify the earliest meaningful drift from the recorded result and replay traces
- [completed] compare the first drift window against the widened robot projectile lifecycle instrumentation and existing AI / impact probes
- [completed] decide whether the new demo closes the remaining projectile-path theory or whether another recorder gap remains
- [completed] add general D2 fire-gate instrumentation around ai_do_actual_firing_stuff so record and replay runs both preserve block reasons before actual fire
- [completed] validate the D2 host build after the logging change
- [completed] rerun the same level 7 replay and confirm the new gate probe appears in the preserved sandbox log

## Notes
- user provided the replay actual result for temp\\input_demo_runtime_wrapper\\d2\\d2_descent2_level7_20260509_140807 and said the matching rngtrace is in the same directory as the recorded demo
- cheap check: find the timestamped .dximdemo plus rngtrace, then compare the earliest RNG / state mismatch against any new weapon_create and probe_weapon_life events for robot-owned non-homing shots
- replayed with -TraceState -TraceRng and confirmed the first player-visible state mismatch at frame 4102 / gt 14268692
- recorded frame 4101 / gt 14266071 contains a player laser hit from weapon 122 that kills robot 156 and awards +750 score; replay misses that kill
- rerunning with -ReplayDebugLog and -KeepSandbox produced results/ai_schedule_probe.log for the replay sandbox
- replay ai sampling shows the missed kill is downstream of an earlier hidden robot-state split, not another same-frame robot-projectile impact branch
- first sampled hidden split is present by frame 3939 / gt 13841397: the recorder has obj 110 in seg 0 plus obj 124 and obj 156 in seg 303, while replay has already drifted to a different obj 110 / 124 / 156 scheduling pattern
- by frame 4101 the recorder kills obj 156 before its AI turn, but replay still has a live obj 156 taking a pre_awareness/process_enter turn in seg 0 while obj 110 is occupying the seg 303 schedule slot
- added a narrow replay-side probe that persists input_demo_log_ai_fire_probe() messages into the replay sandbox log under -ReplayDebugLog
- validated with a focused D2 host build plus a rerun of the same demo; the replay still produces no probe_ai_fire entries in the sandbox log
- this makes the current best explanation: replay never reaches the actual robot-fire branch in the sampled 3939..3941 window, and the later frame 4101 missed kill is downstream of that earlier AI/fire divergence
- next local probe is in ai_do_actual_firing_stuff() around the gating conditions that precede REPLAY_LOG_ACTUAL_FIRE
- new general fire-gate logging now records durable probe_ai_fire_gate lines in the replay sandbox and generic probe_log events during recording via input_demo_append_replay_probe_message
- replay frame 3939 / gt 13841397 shows obj 110 and obj 124 hitting gate_direct_gun_point_null in seg 303 instead of producing any probe_ai_fire entry, and frame 3941 shows the same gate on obj 156
- replay frame 4101 / gt 14266071 shows obj 156 hitting gate_direct_not_ready with next_fire=125829 in seg 0; frame 4102 shows obj 110, obj 124, and obj 156 all hitting gate_direct_not_ready while the visible score mismatch lands
- the new result narrows the root-cause class further: by the earliest sampled hidden split, replay robots are reaching ai_do_actual_firing_stuff() but their fire attempt is blocked by changed local inputs such as gun_point being null, and the later missed kill is downstream of that earlier state drift