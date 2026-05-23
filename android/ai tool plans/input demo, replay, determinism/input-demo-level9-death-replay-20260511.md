# Input demo level 9 death replay investigation

## Goal
- Reproduce the level 9 demo replay failure in `temp/input_demo_runtime_wrapper/d2/d2_descent2_level9_20260511_192615`
- Identify the first divergence around the player death or restart sequence
- Fix the replay/capture gap or game non-determinism with minimal source changes
- Verify the demo replay passes, or leave exact next diagnostics if blocked

## Steps
1. Review current input demo replay notes, memories, and nearby plan files for newest workflow
2. Run or inspect the provided replay artifacts and compare expected, actual, and rngtrace data
3. Locate death/restart capture and replay paths in D1/D2 and shared Android tooling
4. Instrument or patch the narrowest likely source of divergence
5. Run focused validation for the demo and update this plan with results

## Status
- [x] Notes reviewed
- [x] Failure reproduced or existing artifacts confirmed
- [x] First divergence localized
- [x] Fix implemented
- [x] Validation run

## Notes
- Root cause: the raw input that aborts the player death sequence was not represented in replay input frames, so the old recording stayed dead instead of restarting at frame 253
- Fix: recordings serialize a `death_abort` direct command, and replay consumes it while `Player_is_dead`
- Validation: D1/D2 `test_input_demo_replay` passed and D1/D2 game targets built
- Legacy compatibility note: the temporary old-demo inference path was removed, so death/restart validation should use a newly recorded demo that includes the explicit `death_abort` event
