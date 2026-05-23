# Level 6 replay analysis 2026-05-08 123202

## Goal
- determine why the new D2 level 6 save-checkpoint demo fails to replay to completion
- identify whether this is the same hidden checkpoint-state issue as the earlier level 6 failure or a different divergence path

## Steps
- [completed] locate the exact demo file and replay result artifacts for the failing run
- [completed] identify the earliest meaningful replay divergence using result and state-trace outputs
- [completed] compare the divergence against the prior level 6 checkpoint-state failure pattern
- [completed] decide whether the next action is diagnosis-only or a targeted code fix
- [completed] implement and validate a replay-side fix so automap pulses no longer terminate playback early
- [completed] confirm that the late frame-1322 Mercury theft is a downstream symptom, not the first replay RNG drift
- [completed] generate a replay-side RNG trace directly and compare it against the demo's embedded per-frame RNG metadata
- [completed] identify the first default-stream RNG mismatch as a replay-only obj120 awareness roll on frame 561
- [completed] replace the temporary obj120/frame-557-561 probes with generalized replay-side AI awareness and visibility logging and validate that the new probe kinds appear on replay
- [in progress] determine which hidden obj120 suppressor drifted before frame 561, most likely awareness >= PA_PLAYER_COLLISION or previous_visibility != 0 in the recording

## Notes
- initial hypothesis: the replay is diverging much earlier than the previous frame-1045 projectile case, since the final actual result stops at frame 445 instead of 2214, so the controlling mismatch may be a different local checkpoint or control-flow issue
- cheap check: inspect the runtime wrapper result directory and state-trace mismatch output to find the first failing frame and whether the replay stops on a state mismatch, an automation stop, or an unexpected early terminal condition
- confirmed artifacts: the failing demo is android/regression_demos/d2_descent2_level6_20260508_123202.dximdemo and the replay rerun reproduces the failure with result.actual.json under temp/input_demo_runtime_wrapper/d2/d2_descent2_level6_20260508_123202/results plus expected and actual state traces under temp/input_demo_state_traces
- confirmed trace boundary: replayed state matches recorded state exactly through frame 444, the actual state trace stops there, and result.actual.json matches recorded frame 445 state exactly
- frame 445 input is {"p":{"am":1}}, which decodes to an automap pulse in android/app/src/main/cpp/shared/input_demo_controls.cpp
- this is not the earlier hidden checkpoint-state desync: the first failure is a replay control-flow stop, not a gameplay state drift
- likely root cause: d2/main/gamecntl.c ReadControlsReplayFrame() treated the staged automap pulse on frame 445 as a live do_automap() action even though the demo recorded it on the next gameplay frame after returning from automap, which made replay open automap at the wrong time and abort in the next draw loop
- nearby behavior confirms the shape of the stop: d2/main/automap.c immediately closes the automap window when automap_count is still set, so the same pulse can toggle automap inside do_automap() and then unwind back to replay without a parser error while still aborting the replay frame step
- confirmatory experiment: a temp copy of the demo with the lone frame-445 {"p":{"am":1}} pulse replaced by {} replays past the old stop point and reaches frame 2214 instead of stopping at 445
- removing the automap pulse does not make the run fully deterministic: the temp variant reveals a second replay desync starting at frame 1322, where replay first drops player0.secondary_ammo[8] from 2 to 1 and then position diverges on later frames
- practical conclusion for follow-up: the original demo currently contains at least two issues layered together, an early replay control-flow bug on automap pulses and a later gameplay desync that still needs separate investigation once the control-flow stop is out of the way
- implemented fix: in d1/main/gamecntl.c and d2/main/gamecntl.c, replay now ignores staged automap pulses instead of calling do_automap(), so the gameplay frame attached to that pulse still runs normally
- validation: rerunning the original D2 demo now reaches frame_count 2214 in temp/input_demo_runtime_wrapper/d2/d2_descent2_level6_20260508_123202/results/result.actual.json, the replay state trace includes frame 445 and continues cleanly past the old stop boundary, and the first remaining mismatch is back at frame 1322
- remaining issue after the automap-stop fix: the same original demo still diverges later around frame 1322, so the automap bug is fixed but it was only the first blocker in this artifact
- the late thief symptom is now pinned more precisely: replay steals a Mercury on frame 1321, and the later frame-1322 ammo mismatch is just the first visible consequence of that earlier contact
- two local repair hypotheses were tested and rejected, then reverted: treating the automap pulse as a replay pause surrogate did not remove the theft, and preserving saved object next/prev linkage during checkpoint restore did not change the replay outcome
- because the wrapper's -TraceRng path requires a recorded .rngtrace.jsonl sidecar that this demo does not have, the replay-side RNG trace had to be generated by running buildd2/main/dxx-redux-d2-headless.exe directly with -inputdemo-rng-trace
- comparing the generated actual_rngtrace against the demo's embedded per-frame rng.c counts moves the first hidden default-stream mismatch far earlier than frame 1299: the first mismatched frame end-count is frame 561, where the demo expects 8177 and replay ends at 8178
- the exact first extra default-stream consumer is obj120 in d2/main/ai.c line 685 on frame 561, a replay-only near-player awareness roll inside the `((obj_ref & 3) == 0) && !previous_visibility && dist_to_player < F1_0*100` branch
- current replay-only obj120 probe at frames 557-561 shows obj120 is stationary in seg 266 with aware=0, prev_vis=0, and dist already below the 100-unit threshold; frame 561 adds the extra roll only because obj_ref reaches the scheduled modulo-4 slot there
- no compute_vis_and_vec call occurs for obj120 in the probed 557-561 window, so the recorded run's suppressor was not reproduced in replay by then; the remaining plausible hidden differences are that the recording still had awareness >= PA_PLAYER_COLLISION for obj120 or had set previous_visibility nonzero on an earlier path through AI logic
- the frame-445 automap pulse still matters as an independent control-flow bug, but current evidence does not support a long paused automap span as the first hidden gameplay drift in this demo: game_time64 advances continuously through frames 445-469 and the file only shows a single `{"p":{"am":1}}` pulse at frame 445
- the temporary obj120/frame-557-561 `printf` probes in d2/main/ai.c and d2/main/ai2.c were replaced with generalized replay probe kinds written through input_demo_append_replay_probe_message: `probe_ai_schedule`, `probe_ai_awareness_roll`, and `probe_ai_visibility`
- validation rerun after the logging rework still reaches frame_count 2214 in temp/input_demo_runtime_wrapper/d2/d2_descent2_level6_20260508_123202/results/result.actual.json, and ai_schedule_probe.log now includes generic near-player and visibility entries such as `frame=561 ... kind=probe_ai_awareness_roll ... obj=120 ... roll=18504 ... threshold=1687`
