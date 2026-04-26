# Input Demo Phase 4 D1 Runtime Replay Bootstrap

## Goal

Port the existing D2 runtime replay bootstrap to the D1 engine with the same
current constraints:

- `-inputdemo-replay <demo-file>` command-line entry point
- D1-only fixture guard
- `start_mode == "new_level"` only
- per-frame replay injection through the game loop
- final result write plus baseline compare using the shared result helper

## Constraints

- keep replay session parsing in the shared helper
- keep D1 gameplay changes minimal and local to startup and frame-control seams
- mirror D2 behavior where possible so future replay changes stay aligned
- preserve existing desktop and Android D1 builds

## Planned Steps

- [x] Wire D1 targets to link the shared replay helper
- [x] Add D1 inferno replay startup branch and CLI help
- [x] Add D1 game-loop replay injection and final result compare
- [x] Run focused D1 desktop validation
- [x] Run Android native validation
