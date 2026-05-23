# RNG origin compare detail 2026-05-10

## Goal
- improve first nonmatching RNG-call reports so they include state/result and object context
- add a no-emulator fixture for RNG comparer output
- keep this as diagnostics only, with no simulation behavior changes

## Steps
- [completed] inspect current RNG trace comparer and trace schema
- [completed] extend first-mismatch summaries with RNG state/result/object context
- [completed] add no-emulator comparer fixture and wire it into no-infra tests
- [completed] run focused script checks

## Results
- RNG first-difference summaries now include `gt`, object context, RNG state before/after, seed/result, and callsite fields when present
- added `android\tests\test_input_demo_rng_trace_compare.ps1` to cover matching traces and first divergent RNG calls
- registered the RNG comparer fixture as a no-infrastructure test in `android\run_all_tests.ps1`

## Validation
- `android\tests\test_input_demo_rng_trace_compare.ps1`: PASS
- `android\tests\test_input_demo_state_trace_compare.ps1`: PASS after the no-infra wiring change
- scoped `android\run-code-quality.ps1 -Fix -Paths ...`: PASS