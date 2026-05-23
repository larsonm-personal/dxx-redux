# Fix Android Upload Build 2026-05-01

## Goal

Restore the Android upload/build path after the recent demo logging changes without widening scope beyond the failing native compile.

## Planned Steps

- [x] Confirm the exact Android-native compile failures from the upload log.
- [x] Fix the `laser.c` helper ordering issue that breaks C99 compilation.
- [x] Fix the nearby Android format warnings in touched demo logging code.
- [x] Re-run the focused Android native build task that failed.
- [x] Record the result and any remaining warnings.

## Findings

- The hard failure was a C99 ordering bug in `d2/main/laser.c`: `input_demo_weapon_create_probe_active()` called `input_demo_replay_weapon_creation_probe_active()` before that static helper had been declared.
- The Android warnings in the touched logging code came from passing pointer-difference values such as `obj - Objects` to `%d` without an explicit cast on 64-bit Android.
- `d2/main/physics.c` also had a dead local `PhysTime` in the touched area that produced an Android warning.

## Validation

- Focused native task passed with a compatible JVM override:
	- `gradlew ':app:buildCMakeDebug[arm64-v8a]-2'`
- End-to-end build stage passed with the same internal build settings from the failing upload attempt:
	- `1_build-aab.ps1 -BuildType 3 -VersionCode 12751`
	- Output: `android/build-outputs/dxx-redux-internal-20260501-152718-v12751.aab`

## Remaining Warning

- The successful AAB build still reports one unrelated warning in `d2/main/game.c` about `SOUND_AFTERBURNER_IGNITE` narrowing to `char` in `multi_send_sound_function(3, SOUND_AFTERBURNER_IGNITE)`.
- That warning is outside the failing demo-logging changes and was not modified in this tranche.