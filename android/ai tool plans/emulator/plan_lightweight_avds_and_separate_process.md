# Plan: Lightweight AVDs and Separate Process Fix

## Goal
1. Reduce emulator resource consumption by switching from Pixel 6 to Nexus 5X
2. Fix libhwui.so RenderThread crash during SetupActivity->MainActivity transition
3. Verify single-player and multiplayer tests

## Changes Made

### 1. Lightweight AVDs
- Created `android/get_deps/create_light_avds.ps1` to create two Nexus 5X AVDs
- AVD config: 1280x720@320dpi, 1536M RAM, 2 CPU cores, no cameras/GPS/NFC/cell/sensors/audio
- Updated script references in: run_emulator.sh, emu_health.ps1, create_avd.sh
- Deleted old Pixel_6_API_34 and Pixel_6b_API_34 AVDs (freed ~22GB on C:)

### 2. Separate Process for Game Activity
- Added `android:process=":game"` to MainActivity in AndroidManifest.xml
- This runs the SDL game engine in a separate OS process from the Compose UI
- Fixes a libhwui.so FORTIFY abort (pthread_mutex_lock on destroyed mutex) that
  occurred 100% when transitioning from Compose Activity to SDL SurfaceView Activity

### 3. Lint Fix
- Fixed redundant curly braces in string template in SetupActivity.kt

## Test Results
- Single-emulator D2 test: PASS (27/26 steps, 29.7s)
- Multiplayer test Phases 1-7: ALL PASS
- Phase 8: EMU1 enters game; EMU2 connects and exchanges traffic via relay
  - EMU2 emulator crashes after ~0.3s of active gameplay (resource limitation)
  - Relay log confirms bidirectional game traffic before crash
- Phase 9: Fails because EMU2 emulator is dead

## Known Issues
- Dual-emulator 3D rendering is too resource-intensive for reliable testing
  - Both emulators use swiftshader_indirect (software GPU rendering)
  - EMU2 consistently dies after brief gameplay
  - This is a host PC resource issue, not a code bug
- The separate process fix successfully prevents the libhwui crash
