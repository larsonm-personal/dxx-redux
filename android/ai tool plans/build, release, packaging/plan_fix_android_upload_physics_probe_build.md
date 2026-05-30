# Fix Android Upload Physics Probe Build

## Goal

Restore the Android upload/build pipeline by fixing the D2 physics replay probe compile break without widening scope.

## Planned Steps

- [x] Confirm the failing file and missing declaration path from the Android build log.
- [x] Add the minimal include or dependency needed for the D2 physics replay probe.
- [x] Re-run the focused Android native build path used by the upload script.
- [x] Update the plan with the final validation result.

## Notes

- The failing symbols are `input_demo_replay_is_loaded()` and `input_demo_replay_next_frame_index()` from probe code in `d2/main/physics.c`.
- The root cause appears to be a missing header include, not a broken implementation.
- Validation: `:app:buildCMakeDebug[arm64-v8a]-2` succeeded after the include fix, and `1_build-aab.ps1 -BuildType 3 -VersionCode 12490` produced a new internal AAB successfully.