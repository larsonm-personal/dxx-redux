# Input Demo Phase 5 Result Compare Tranche

## Goal

Add the first shared result reader and comparator, then use it from replay after
writing the actual result JSON file.

This tranche should:

- extend the shared result helper with read/compare APIs
- keep the compare logic in shared code rather than in D2 gameplay code
- allow current minimal recorder baselines to compare against richer actual
  replay results by making optional sections baseline-driven for now
- log replay match or mismatch in the D2 runtime after actual result output

This tranche does not yet add a standalone diff script, D1 runtime comparison,
or fully strict comparison of richer optional sections when the baseline omits
them.

## Constraints

- keep file parsing, sparse default handling, and mismatch formatting in the
  shared helper tree
- reject unknown result keys during parsing so the fixture schema stays bounded
- compare mandatory top-level fields strictly
- compare optional fields and sections only when present in the expected
  baseline for this incremental slice

## Planned Steps

- [x] Extend `input_demo_result` with read/compare APIs
- [x] Extend the focused host result test to cover read/compare behavior
- [x] Hook D2 replay completion to compare baseline and actual results
- [x] Run focused D2 validation
- [x] Run Android native validation

## Exit Criteria

- Shared code can read result JSON back into a result struct
- Shared code can report a readable mismatch for compared fields
- D2 replay logs whether the baseline matched the actual result file

## Progress on 2026-04-25

- extended `input_demo_result.h/cpp` with result read and compare APIs while keeping the C-facing pure-data struct boundary intact
- added `has_game_time64` so the shared helper can distinguish omitted `gt` from a present final game time and use baseline-driven optional comparison
- parser validation now rejects unknown top-level and section keys for the currently supported result shape
- extended `android/tests/test_input_demo_result.cpp` to cover write, read, struct round-trip compare, baseline-driven compare of a minimal expected result against a richer actual result, and a labeled mismatch case for `p0.sc`
- updated D2 replay completion in `d2/main/game.c` to compare the embedded baseline trailer against the actual result after writing it and to log either a match or a readable mismatch
- validation passed after formatting with `run-windows-build.ps1 -Target d2`, `buildd2\maths\test_input_demo_result.exe`, `buildd2\maths\test_input_demo_recorder.exe`, `buildd2\maths\test_input_demo_replay.exe`, and `android\gradlew.bat :app:externalNativeBuildDebug --no-daemon`