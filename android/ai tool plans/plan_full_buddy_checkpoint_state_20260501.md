# Plan: full buddy checkpoint state

Date: 2026-05-01

Goal: move the full guidebot checkpoint state needed for input demo replay out into the demo checkpoint JSON, restore it after the embedded save loads, and validate the change against the known checkpoint replay desync.

## Phases

| phase | task | status |
|---|---|---|
| 1 | Enumerate the full escort and buddy AI state that replay currently reconstructs or suppresses after checkpoint load. | completed |
| 2 | Expand the shared demo checkpoint fixture/JSON parsing so it can carry the full buddy checkpoint state. | completed |
| 3 | Restore that full buddy checkpoint state in D2 after loading the embedded save and remove replay-only escort compensation that becomes obsolete. | completed |
| 4 | Rebuild the touched host target and rerun the failing checkpoint replay to verify the first RNG mismatch is gone or moves. | completed |
| 5 | Update notes with the new behavior and any remaining desync if the demo still diverges. | completed |

## Notes

- Keep the normal DGSS save format untouched; demo-only buddy fidelity should live in checkpoint JSON.
- The current failing artifact is `android/temp_game_logs/d2_descent2_level2_20260501_085718.dximdemo`.
- The working hypothesis is that the partial checkpoint restore forced replay-only escort suppression in `d2/main/escort.c`, and a full buddy restore should let replay use the normal live branch logic.

## Findings

- The demo checkpoint escort JSON now carries the full escort runtime globals that replay previously reconstructed or partially relied on DGSS to preserve: escort kill and goal state, buddy message suppression and timing state, marker selection state, and network owner in addition to the existing timing fields.
- D2 replay now restores that checkpoint escort state after `state_restore_all_sub()` and no longer uses the replay-only checkpoint suppression branches in `time_to_visit_player()` or the follow-up early return path.
- Focused validation passed after the change: `run-windows-build.ps1 -Target d2`, `test_input_demo_fixture.exe`, and `test_input_demo_replay.exe` all succeeded.
- Replaying `android/temp_game_logs/d2_descent2_level2_20260501_085718.dximdemo` no longer hits the old frame-28 guidebot desync. Replay now takes the buddy `create_path_to_player` branch and writes the expected `0/4` escort path instead of staying stuck on `0/53`.
- The old early failure is gone, but the demo still diverges later. The first RNG mismatch moved to frame 296 in `d2/main/ai.c:create_awareness_event`, the first visible state mismatch moved to frame 645 on `player0.score` (`32900` vs `33150`), and the final result now differs only at `position.x` by 2 units.