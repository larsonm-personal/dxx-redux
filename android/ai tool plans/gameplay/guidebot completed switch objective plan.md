# Guide-Bot completed switch objective plan

## Goal

Diagnose and fix the D2 co-op level 20 case where a destroyed switch remains the Guide-Bot and automap next objective

## Plan

- [x] Correlate the supplied debug log with objective and Guide-Bot state transitions
- [x] Trace switch completion, multiplayer synchronization, Guide-Bot navigation, and automap objective selection
- [x] Implement the smallest shared-state fix and add focused diagnostic coverage if the current log is insufficient
- [x] Add or extend a regression test for completed switch objectives
- [x] Run scoped formatting, tests, and the required build verification

## Notes

- Preserve unrelated user changes already present in the worktree
- Keep D2 engine edits minimal and retain desktop platform behavior
- The log shows trigger 18 was initially skipped, the Guide-Bot pursued trigger
  12 in segment 44, then regressed to trigger 18 in segment 172 after its live
  wall state changed
- Trigger activation now records the matching canonical trigger immediately,
  including when an earlier objective is active and when the activating player
  is not the local Guide-Bot owner
- The focused emulator test passed all 30 steps on the isolated secondary AVD,
  including persistent completion of trigger 18 and selection of trigger 12
- D1 and D2 Windows builds passed, Android debug builds passed for all three
  ABIs, and scoped code quality passed
