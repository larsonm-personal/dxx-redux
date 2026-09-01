# Cooperative join follow-up

## Goal

Diagnose the remaining cooperative join failure in the 16:14 host and client logs, then complete and verify the initial-sync recovery fix.

## Plan

1. [x] Identify peer roles and extract only protocol-transition, sync, retry, dump, and disconnect events from both logs.
2. [x] Compare the new failure against the initial-sync retry implementation and trace the remaining state transition.
3. [x] Implement the smallest paired D1/D2 fix with focused regression coverage.
4. [x] Run scoped quality checks, native tests, Android build and unit tests, and Windows D1/D2 builds.

## Verification

- The host's initial-sync retry fired at 16:15:00.612. The client accepted it at 16:15:00.880, reached `PLAYING`, received all 964 host-save chunks, restored the save, and rendered the restored level.
- The client had been applying the completed save from inside the UDP packet handler. The restore tears down and rebuilds the level while the packet parser still owns the old frame state. Client save, rewind, and level-restart applies now wait for the next game-frame boundary.
- Added lifecycle, setup-navigation, and raw game-process diagnostics so a later export distinguishes process loss, ordinary backgrounding, and the otherwise-unlogged left-edge setup gesture.
- Passed scoped `android/run-code-quality.ps1 -Fix`.
- Passed `test_net_udp_reconnect_auth`, `test_net_udp_initial_sync_retry`, and `test_multi_save_transfer_policy` through CTest.
- Passed `NetworkEventsOverlayTest` through `:app:testDebugUnitTest` with JDK 21.
- Passed `:app:assembleDebug` for arm64-v8a, armeabi-v7a, and x86_64.
- Passed Windows D1 and D2 builds through `run-windows-build.ps1`.
