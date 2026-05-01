# Input Demo Frame Events Plan

## Goal

Add durable per-frame `.dximdemo` event logging so recordings capture gameplay
events like robot damage, impacts, weapon creation, and score changes directly
in the file instead of only in replay-side transient logs.

## Phases

1. [completed] Extend the shared frame schema and recorder session with an
   optional per-frame `events` array that round-trips through parse and write
2. [completed] Add recorder append APIs and wire D2 event capture into the
   existing score, collision, damage, and weapon creation hooks
3. [completed] Add focused shared tests for event round-trip and recorder
   flush output, then run the narrow recorder or fixture validation targets
4. [not_started] Re-run the target non-headless D2 replay path if the shared
   validation passes, to confirm the recorded demo now contains the missing
   durable evidence around the first mismatch window

## Notes

- Keep existing frame-start input, RNG, and state timing unchanged
- Start with durable event objects before adding larger per-frame robot arrays
- Use grep-friendly JSON objects with stable field names and ordered keys
- Validation completed so far: `cmake --build buildd2 --target dxx-redux-d2 dxx-redux-d2-headless test_input_demo_recorder test_input_demo_fixture -- -k 10`, `buildd2\\maths\\test_input_demo_recorder.exe`, and `buildd2\\maths\\test_input_demo_fixture.exe`