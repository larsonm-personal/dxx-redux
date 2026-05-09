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
