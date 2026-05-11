# First divergent object detail 2026-05-10

## Goal
- extend state-trace diagnostics beyond aggregate object hashes so replay comparisons can name a likely changed object before new demos are recorded
- keep the data compact enough for per-frame traces
- preserve D1/D2 parity and existing trace backward compatibility

## Steps
- [completed] inspect current object diagnostic schema and comparer behavior
- [completed] add compact object-slot bucket fields for live object lanes
- [completed] update comparer summaries and no-emulator fixtures
- [completed] run focused builds, tests, and scoped code quality

## Results
- added `highest_object_index`, `object_slot_bucket_size`, `object_slot_counts[]`, and `object_slot_hashes[]` to state-trace diagnostics
- D1 and D2 now hash live object state into 32-slot object-index buckets, keeping per-frame traces compact while narrowing aggregate object hash divergence
- the comparer now labels object slot bucket mismatches as `object_state` and appends `object_slot_range=start-end`
- the no-emulator comparer fixture now covers both segment-list order mismatches and object-slot bucket mismatches

## Validation
- `android\tests\test_input_demo_state_trace_compare.ps1`: PASS
- `cmake --build buildd2 --target test_input_demo_recorder`: PASS
- `buildd2\maths\test_input_demo_recorder.exe`: PASS
- `cmake --build buildd1 --target test_input_demo_recorder`: PASS
- `buildd1\maths\test_input_demo_recorder.exe`: PASS
- `cmake --build buildd2 --target dxx-redux-d2`: PASS
- `cmake --build buildd1 --target dxx-redux-d1`: PASS
- scoped `android\run-code-quality.ps1 -Fix -Paths ...`: PASS
- existing `102738` state-trace compare: still reports known frame 241 motion divergence on old traces