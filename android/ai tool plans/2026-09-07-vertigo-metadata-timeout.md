# Vertigo CD metadata timeout

1. Reproduce the single failed d2x.mn2 descriptor from run 20260907_164450
2. Determine whether the timeout reflects stalled work or a whole-campaign timeout
3. Correct progress-write throttling and verify the Vertigo CD scan
4. Run relevant worker tests and preserve existing generated data

The run completed 132 of 133 sources successfully. Only the Vertigo CD descriptor exceeded the 30-second CD timeout. The similarly named Vertigo Missions archive is a different D1 mission.

## Cause and change

The prior worker recovery change enabled existing native progress checkpoints. Their throttle exempted every new task and every task completion, so fast route-visibility subtasks repeatedly truncated and rewrote the checkpoint file. Vertigo timed out with checkpoints enabled but scanned all 23 levels in 3.97 seconds with them disabled.

All intermediate progress writes now share the existing 100 ms interval, including task changes and completions. Observed progress is updated even when a write is skipped, preserving correct task identity. The level and request completion checkpoints remain immediate. No timeout was increased and no mission was excluded.

## Validation complete

- Both Windows engines build
- All 49 native CTest tests pass
- New test_vertigo_metadata_checkpoints.ps1 passes: all 23 levels, normal 30-second CD timeout, matching final request checkpoint
- Direct Vertigo scan with throttled checkpoints completes in 3.48 seconds; checkpoint-enabled and checkpoint-disabled metadata are identical
- Full host regeneration passes all 133 sources (5 CD collections and 128 archives), zero failures, plus both built-in campaigns; 477.6 seconds
- Scoped code quality and git diff checks pass

Full validation used NoRegressionCopy and preserved the user's generated metadata. Android device behavior was not tested.

Evidence: android/temp/vertigo_checkpoint_full_validation/summary.json, temp/vertigo_checkpoint_full_validation.log, android/temp/test_vertigo_metadata_checkpoints/run_20260907_171114_477, temp/vertigo_checkpoint_ctest.log, and temp/vertigo_timeout_final_build.log
