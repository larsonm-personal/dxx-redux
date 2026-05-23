# Level 6 replay analysis 2026-05-08 193201

## Goal
- determine why the new D2 level 6 demo fails to replay
- decide whether the earliest hidden drift matches the recent level 6 AI awareness and visibility RNG failure pattern or a different replay bug class

## Steps
- [completed] generate replay artifacts for the failing demo with replay probe logging enabled
- [completed] identify the earliest meaningful replay mismatch from result, state trace, and RNG trace outputs
- [completed] inspect `ai_schedule_probe.log` around the earliest mismatch to see whether `probe_ai_awareness_roll`, `probe_ai_visibility`, or `probe_ai_schedule` explain the first extra RNG consumer
- [completed] decide whether the next action is diagnosis-only or a targeted replay fix
- [completed] add generic record-side AI schedule and RNG-branch captures so future recordings carry enough data to compare against replay without frame- or obj-specific probes

## Notes
- initial hypothesis: because this is another D2 level 6 failure immediately after the previous awareness-roll issue, the new demo may diverge from the same class of hidden AI state drift, likely a replay-only awareness roll or visibility transition that consumes default RNG before the later visible failure
- cheap check: run the failing demo through the headless D2 replay path with `-inputdemo-debug-log` and `-inputdemo-rng-trace`, then compare the first RNG mismatch frame against the new generalized `probe_ai_awareness_roll` and `probe_ai_visibility` entries in `ai_schedule_probe.log`
- replay output first reports the RNG mismatch at frame 296, but the mismatch line uses gt 12695304, which is the frame 295 checkpoint boundary, so the first hidden drift happens during frame 295 rather than after frame 296 starts
- the recorded frame 296 checkpoint expects rng state 3014409015 with call count 73758; replay actual_rngtrace reaches that same state at frame 295 call 73758 on obj114, then immediately consumes one extra call at frame 295 call 73759 on obj115 in `d2/main/ai.c` line 542
- `d2/main/ai.c` line 542 is `agitation_path_trigger_roll = d_rand();`, so this new demo does not match the previous frame-561 near-player awareness-roll failure exactly
- the generalized awareness and visibility probes were still useful: the frame 296 `probe_ai_awareness_roll` for obj41 exists in the recording too, so it is not the replay-only extra RNG consumer in this demo
- current diagnosis: this is the same broad class of hidden AI RNG drift, but the first replay-only call is an unlogged agitation-path trigger roll for obj115 during frame 295, not a replay-only awareness or visibility transition
- validated follow-up instrumentation: replay now logs failed `agitation_path_gate` evaluations for all robots, not just passing gates or the old obj95 special case, and the rebuilt replay captures the exact extra call as `frame=295 ... kind=agitation_path_gate ... obj=115 ... calls=73759 state=3186239652`
- obj114 and obj115 both emit failed agitation-path gate lines on replay for frames 290 through 296, so the frame-295 divergence is not a one-frame change in the agitation gate math itself
- extra probe fix: `skip_return_pre/post` schedule logs now bypass the distance gate by treating `obj_ref < 0` as the explicit top-of-frame skip sentinel, but the rebuilt replay still emits no frame-295 `skip_return` line for obj115
- updated local hypothesis: the missing recording-side suppressor is earlier than the line-542 agitation roll and is not reproduced in replay after the new skip probe coverage; the most plausible remaining candidates are a recording-only top-of-frame `SKIP_AI_COUNT` return or another pre-roll `do_ai_frame` suppression for obj115 on frame 295
- validation note: `run-windows-build.ps1 -Target d2` and the headless replay both complete correctly when launched through a clean child process with redirected stdout/stderr and status files, even though the VS Code terminal wrapper times out instead of returning control
- new tranche goal: capture record-side AI control flow generically enough that future hidden RNG drifts can be diagnosed from the recorded `.dximdemo` alone, especially whether a robot was processed or skipped before hitting a default-RNG branch and whether near-player, visibility, or agitation gates rolled during recording
- implemented record-side capture: `probe_ai_schedule` now writes JSON events for the same interesting AI slice already used by replay schedule logging, and the existing `probe_ai_awareness_roll`, `probe_ai_visibility`, and `probe_ai_agitation_path_gate` helpers now emit record-side JSON too instead of leaving those details replay-only
- validation: bounded child-process `run-windows-build.ps1 -Target d2` completed with `EXIT: 0`, and `android/tests/test_input_demo_runtime_smoke.ps1 -Game d2` also completed with `EXIT: 0`; a targeted `run-code-quality.ps1 -Fix` attempt fell back to repo-wide checks because `-Paths` was passed as a literal string and then failed only on existing unrelated ktlint max-line-length findings in `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
