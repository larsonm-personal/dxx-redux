# Fix suite failures from report_20260926_141249

1. Inspect evidence for CUE/ISO, physical route cases, D2 replay, and route regeneration failures
2. Reproduce the failing cases and correct their underlying build, runner, or engine causes
3. Run the affected integration owners to completion and relevant native builds/tests
4. Apply scoped code quality and record verification and any remaining limitations

Initial evidence: CUE/ISO CTest discovers unbuilt FluidSynth tests; route owner reports three failing cases; D2 replay reports multiple failures; route audit fails the Ironstar physical completion check

## Fixes and verification

- FluidSynth registers upstream tests unconditionally even though its test targets are excluded from the build. Disable that registration in the pinned dependency before adding the library subdirectory. The CUE/ISO integration owner passes all 52 tests
- The full-radius route actor represents the player following Guide-Bot, but trigger calls supplied the robot identity. D1-format triggers correctly reject robots, so FirstStrike L21 and Ironstar L6 repeatedly crossed without firing. Use the sandboxed player identity after the physical crossing or firing-position proof. FirstStrike passes twice and the complete route regeneration audit passes, including all repeated Ironstar and FFYL door cases
- Die Hard L6 completed correctly, including a verified shot at wall 70. Its assertion unnecessarily fixed the target segment to 568, although the current verified shot targets segment 566. Retain the actor, wall and completion-order checks while allowing the verified ray's target segment to vary
- The Original-routing comparison crashed with 0xc0000005 in input_demo_recorder_start while reading checkpoint_save_name. Engine headers leave MSVC byte packing active, whereas the recorder implementation used default packing. Pin the shared settings structure to 8-byte packing. Both Original-routing levels pass twice; D1 and D2 builds pass without compiler warnings; fixture, recorder and replay CTests pass in both engines (6 total)
- The route integration owner passes all 24 seed-268 cases in android/temp/route_regression_cases/run_20260926_151548_371/summary.json. Scoped code quality and git diff --check pass

## Remaining D2 recordings

The complete D2 corpus still reports 9 failures out of 11 in temp/suite_fix_d2_replays.log. Keep these failures and the original recordings intact

For d2_descent2_level9_20260511_215654.dximdemo, the saved header identifies arm64 build 13180, revision b5222e6, before the current Guide-Bot routing implementation. State comparison first reports robot-state divergence at frame 52. The recorded RNG trace includes Guide-Bot object 166/id 33 path-generation draws at frame 51 that the current run does not make. Source-function renames also cause the strict RNG comparison to report earlier textual differences; those are distinct from the changed draws

A second isolated current-engine run matches the first run's terminal result exactly with -StrictComparison -ReferenceResultPath. It still differs from the historical recording by x=34, y=4, z=4 fixed-point units. This establishes repeatability for this case, not correctness or repeatability of the other eight failures. Logs: temp/suite_fix_d2_trace.log and temp/suite_fix_d2_repeatability.log

Further D2 engine investigation or fresh recordings are needed; do not replace expected terminal values with replay output or introduce historical replay compensation
