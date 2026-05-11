# Object list order detection 2026-05-10

## Goal
- add reusable detection for object and segment object-list order divergence before new demos are recorded
- add focused round-trip fixtures that catch allocator or segment-list order drift across save/checkpoint restore
- keep changes scoped to input-demo diagnostics and host tests where possible

## Steps
- [completed] inspect current state trace, comparer, and host test patterns
- [completed] add segment object-list order hashes and first divergent object/list reporting
- [completed] add host round-trip fixtures for segment object-list diagnostics
- [completed] run focused build/tests and update this plan with results

## Results
- added `segment_object_list_count`, `segment_object_list_hash`, and `segment_object_link_error_count` to state-trace diagnostics
- mirrored segment linked-list hashing in both D1 and D2 input-demo capture hooks
- updated the state-trace comparer to label list-order mismatches as `object_list_order` and print compact expected/actual diag summaries
- added a no-emulator comparer fixture for object-list mismatch reporting
- extended recorder/state-trace round-trip tests to cover the new diagnostic fields

## Validation
- `android\tests\test_input_demo_state_trace_compare.ps1`: PASS
- `cmake --build buildd2 --target test_input_demo_recorder`: PASS
- `buildd2\maths\test_input_demo_recorder.exe`: PASS
- `cmake --build buildd1 --target test_input_demo_recorder`: PASS
- `buildd1\maths\test_input_demo_recorder.exe`: PASS
- `cmake --build buildd2 --target dxx-redux-d2`: PASS
- `cmake --build buildd1 --target dxx-redux-d1`: PASS
- `android\run-code-quality.ps1 -Fix -Paths ...`: PASS
- existing `102738` state-trace compare: still reports known frame 241 motion divergence on old traces