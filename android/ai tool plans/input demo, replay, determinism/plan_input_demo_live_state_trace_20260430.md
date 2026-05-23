# Input Demo Live State Trace Plan

## Goal

Make replay runs write one grep-friendly JSONL line per replayed frame with the
same check values already stored under each `.dximdemo` frame `state` object.
This should replace ad hoc replay probe strings for routine desync triage and
make it faster to find the first frame where host replay diverges from the
recording.

## Desired Replay Output

Add a replay flag such as:

```powershell
-inputdemo-state-log temp\input_demo_state_traces\run.actual_state.jsonl
```

Each output line should use the same shape as the extractor script:

```jsonl
{"type":"frame_state","source":"replay","f":810,"ft":2622,"rng":{"s":2636896831,"c":22066},"state":{"game_time64":2233467}}
```

The replay-side sample should be taken at the same phase as the current replay
state comparison: after replay input and RNG state are prepared for frame `f`,
before the frame simulation mutates gameplay state. The `state` object should be
serialized by the same shared result serializer used by the `.dximdemo` frame
state and final result trailer.

## Helper Scripts

1. [completed] `android/tests/export_input_demo_state_trace.ps1`
   extracts expected per-frame `state`, normalized `ft`, and recorded RNG data
   from a `.dximdemo` into `temp/input_demo_state_traces/*.expected_state.jsonl`.
2. [completed] `android/tests/compare_input_demo_state_trace.ps1`
   compares two state JSONL streams and reports the first mismatching frame and
   JSON path. It can compare a generated expected trace against either a future
   replay trace or the source `.dximdemo` itself.
3. [completed] Extend `android/tests/run_input_demo_replay.ps1` with
   `-StateLogPath`, `-TraceState`, and `-CompareStateTrace`. The wrapper now
   places actual logs under `temp/input_demo_state_traces/` by default, prints
   the path, and can run the compare helper automatically.

## Engine Work

1. [completed] Add a small shared writer helper for frame-state JSONL so D1 and
   D2 do not grow separate serializers.
2. [completed] Add `-inputdemo-state-log <path>` parsing in D1, D2, and the D2
   headless runner, guarded so it only writes while input-demo replay is active.
3. [completed] Emit one `frame_state` line per replay frame at the existing
   replay compare phase. The trace now carries `f`, effective `ft`, replay-frame
   RNG metadata, and the serialized actual `state` object.
4. [completed] Flush or close the state trace at replay completion and on replay
   abort, keeping failures visible in console output.
5. [completed] Add focused host validation: the runtime smoke now fails if replay
   does not write frame-state JSONL, and the wrapper can compare an actual trace
   against extracted expected state automatically.

## Analysis Workflow

1. Extract expected state from the recorded demo.
2. Replay with state trace enabled and keep the sandbox when investigating.
3. Compare expected and actual traces to find the first visible mismatch.
4. Inspect nearby frame events, powerup summaries, and `.rngtrace.jsonl` by
   frame and RNG call count. Do not interpolate between frames.
5. Add narrow semantic events only when the state trace points to a subsystem
   but not the specific object lifecycle step.
