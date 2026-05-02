# Input Demo Replay RNG Trace Plan

## Goal

Make replay runs write a `.rngtrace.jsonl` stream that matches the recording-side
sidecar format exactly enough for a mechanical line-by-line diff to report the
first divergent RNG event. Keep the existing `.dximdemo` frame `state` versus
replay `frame_state` comparison as the frame-level locator, and use the replay
RNG trace to narrow the cause to the exact callsite and sequence number.

## Desired Replay Output

Add a replay flag such as:

```powershell
-inputdemo-rng-trace temp\input_demo_state_traces\run.actual_rngtrace.jsonl
```

The replay output should reuse the same JSONL writer as recording so the output
shape, key order, escaping, and newline rules stay identical:

```jsonl
{"type":"meta","version":1,"events":1234,"truncated":false}
{"type":"rand","seq":0,"frame":0,"gt":0,"call_count":1,"state_before":1,"state_after":1103527590,"result":16838,"line":127,"file":"rand.c","func":"d_rand_annotated"}
```

The only intended difference between recording and replay files should be the
path chosen by the caller. When replay stays deterministic, the content should
match character-for-character.

## Engine Work

1. [completed] Refactor the shared RNG trace writer so recording can keep writing
   `<demo>.rngtrace.jsonl` while replay can write to an explicit caller path.
2. [completed] Add `-inputdemo-rng-trace <path>` parsing in D1, D2, and the D2
   headless runner, mirroring `-inputdemo-state-log`.
3. [completed] Start RNG trace capture only for replay runs that requested the
   flag, and flush or close the trace at replay completion and replay abort.
4. [completed] Update replay frame preparation so RNG trace mode restores the
   recorded per-frame `call_count` baseline instead of resetting the simulation
   counter per frame.
5. [completed] Set replay RNG trace frame context at the same frame boundary used
   by recording so `frame` and `gt` align with the recorded sidecar.
6. [completed] Normalize replay trace `file` paths to `/` separators so Windows
   host replays match the existing sidecar path format.
7. [completed] Suppress pre-context startup RNG events so replay logging starts
   with the same first in-frame event sequence as recording.

## Wrapper Work

1. [completed] Extend `android/tests/run_input_demo_replay.ps1` with
   `-TraceRng`, `-RngLogPath`, and optional auto-compare wiring.
2. [completed] Add `android/tests/compare_input_demo_rng_trace.ps1` to report the
   first differing behavioral line and nearby context between recorded and replay
   traces, while still calling out meta drift and source-line-only mismatches.

## Validation

1. [planned] Add or extend a focused host smoke path so a short freshly recorded
   demo can produce both recorder and replay RNG traces and compare them.
2. [completed] Run the relevant D1 and D2 builds, the focused smoke path, and the
   replay wrapper trace comparison.
3. [completed] Re-run the level-2 regression workflow and use the first changed
   RNG line plus the first changed replay `frame_state` to localize the desync.