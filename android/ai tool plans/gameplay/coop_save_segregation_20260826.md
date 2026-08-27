# Co-op save segregation

## Goal

Keep single-player and co-op saves from appearing in the same launcher resume slot and replace the misleading mission-load error with a clear co-op-save warning.

## Plan

- [x] Trace launcher save discovery, save metadata, and the game load failure path in both D1 and D2
- [x] Add the co-op-save warning at the earliest reliable validation point, with matching D1 and D2 behavior
- [x] Key launcher save sets by mission and play mode so single-player and co-op saves remain separate
- [x] Add or extend high-level regression coverage for warning text and save-set selection
- [x] Run scoped code quality, relevant tests, and CMake/Android build verification

## Progress

- Plan created
- Confirmed save-set storage is already keyed separately as `single/<pilot>/<mission>` and `coop/<mission>`
- Added an engine-side guard against parsing `.mgN` co-op headers as single-player saves
- Excluded every `.mgN` path from launcher resume ranking, independent of metadata callsign
- Added paired D1/D2 source contracts proving the warning precedes header parsing and the launcher filter precedes candidate publication
- Scoped code quality, persistence contracts, both Windows builds, all three Android debug ABIs, and APK assembly passed
- The full Android unit run completed 955 tests with two unrelated failures in the user's concurrent touch-edge work; save/resume suites did not fail
- Follow-up: simplify the engine warning so `state.c` does not describe launcher menu navigation
