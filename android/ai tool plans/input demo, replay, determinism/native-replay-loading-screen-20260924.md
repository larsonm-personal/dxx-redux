# Native replay loading-screen hang

User report: d1x-redux used by tests commonly remains at Prepare for Descent

- Live PID 29880 was actively writing a growing state trace, so that instance
  was replaying behind the last startup screen, not stuck in loading
- Both SDL event loops bypass all window drawing whenever no-render replay is
  loaded, including when a modal/pause window owns the foreground. This can hide
  the reason a replay stops advancing
- Keep game rendering bypassed, display throttled replay frame progress, and let
  frontmost menus/dialogs use normal drawing/presentation. Preserve SIM RNG
- Validate using real native/imported replay processes, pause/resume interaction,
  and complete state/RNG comparisons to the pinned prior capture
- Do not modify the capture helper during the frozen full corpus; its historical
  binary/helper manifest must remain true. Retain diagnostic sandboxes manually

Implementation and verification:

- Shared fast replay helper displays wall-clock-throttled frame progress without
  changing simulation time or RNG. Both SDL event loops draw modal windows while
  suppressing only gameplay draw events in no-render mode
- The first integration run caught an incorrectly placed draw filter in input
  dispatch. Corrected before qualification; initial failed evidence retained at
  temp/d1-replay-progress-native
- Corrected native Windows integration passes pause, stable paused frame,
  resume, all 2006 frames, and process exit 0. Complete decoded state/RNG traces
  and final result are byte-identical to the prior current native capture
  Evidence: temp/d1-replay-progress-native-corrected/window-report.json and
  trace-verification.json
- Both Windows engines build; scoped quality passes; all 53 D1 and 61 D2 host
  tests pass. Logs: temp/d1-replay-progress-corrected-{quality,build}.log and
  temp/d1-replay-progress-ctest-{d1,d2}.log
- Reusable Windows integration: android/tests/test_replay_window_progress.py
  Targets only the isolated test executable's window and retains its sandbox
- Frozen historical corpus continues with its pinned old executable, so its
  existing window will retain the old loading picture until that capture ends
- Android runtime/build validation of this UI change is pending; the active
  historical native replay prevents the Android cleanup/build preflight

- Imported D1 capture also completes all 2006 frames with exit 0 and identical
  decoded state/RNG traces and result JSON versus the previous imported capture
  Evidence: temp/d1-replay-progress-imported/trace-verification.json
- Its initial interaction test failed because D2 intentionally filters Pause
  during input replay (game.c EVENT_KEY_COMMAND). Preserve that failed report;
  the reusable test now checks progress/completion for D2, pause/resume for D1
- At 16:15 the historical level 7 native-a capture completed and native-repeat
  began. This independently confirms the observed old loading screen was stale

- Final D2 progress/completion integration passes with exit 0; its complete
  decoded state/RNG traces and result JSON remain byte-identical to the prior
  imported capture. Evidence: temp/d1-replay-progress-imported-progress/
  window-report.json and trace-verification.json
- Scope: fixes stale loading UI and hidden modal/pause UI in fast windowed
  replays. Does not establish that every possible startup stall is resolved

Throughput follow-up:

- Same current native binary, level 15 recording, accelerated windowed no-render:
  no trace 5.092 seconds / 393.95 fps; state+RNG trace 110.951 seconds / 18.08 fps
  Runner-reported elapsed times include engine startup, exclude wrapper setup
- Traces serialize 742,577,818 bytes for 2006 frames, compressed to 64,111,051
  bytes in the earlier identical native capture. State/object/world JSON builds
  and serialization run on the replay thread, with synchronous per-record flush
- Final result JSON is identical with and without traces. Evidence:
  temp/d1-replay-throughput/{no-trace,traced}.log and corresponding result JSON
- Separate ad hoc process-exit timing was invalid: windowed replay writes its
  result then remains open, and the existing runner closes it. The direct launch
  completed its result but timed out waiting for process exit; do not use that
  timeout as a replay-throughput or startup-hang finding
- This demonstrates diagnostic capture cost, not a 25-fps windowed cap. Exact
  console-vs-window throughput was not compared: helper currently permits the
  console runner only for native D2 accelerated checkpoint replays, not D1 mode
