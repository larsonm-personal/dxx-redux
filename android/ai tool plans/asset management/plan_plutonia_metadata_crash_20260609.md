# Plutonia metadata crash

## Goal
Find why level metadata analysis crashes for game_data/plutonia.zip.

## Plan
- [x] Re-read project instructions.
- [x] Inspect plutonia.zip contents and mission descriptors.
- [x] Trace the launcher level-metadata analysis path for ZIP missions.
- [x] Reproduce or isolate the failing HOG/level entry.
- [x] Implement a focused fix or report the precise data issue.
- [x] Run focused verification.

## Verification
- Ran scoped code quality on `android/app/src/main/cpp/jni_level_metadata.cpp`.
- Ran `:app:externalNativeBuildDebug`.
- Ran `:app:testDebugUnitTest --tests com.dxxredux.app.LevelMetadataTargetsTest`.
- Could not run the original on-device analysis because no adb device/emulator was connected.
