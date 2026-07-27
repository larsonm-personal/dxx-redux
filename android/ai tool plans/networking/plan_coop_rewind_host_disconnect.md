# Coop rewind host disconnect

## Goal

Diagnose and fix the coop rewind regression where restoring a rewind removes the host while leaving the client in game.

## Plan

- [x] Read repository instructions and inspect the supplied log around the rewind
- [x] Trace player matching and network state changes through D1 and D2 restore
- [x] Implement the smallest shared fix with a regression test
- [x] Run scoped formatting, relevant tests, Android build, and Windows CMake build verification
- [x] Record findings and completed verification here

## Initial evidence

- The rewind save contains two connected players: `touch` in slot 0 and `Player68` in slot 1
- During restore, current slot 0 still contains connected host `touch`, while slot 1 has already become disconnected
- Restore nevertheless reports `P0 'touch' not found in save` and reduces the live count to one
- This places the failure in player remapping after level synchronization, not in thief ownership or pickup serialization

## Root cause

- Multiplayer rewind restored the host before it queued the authoritative snapshot transfer to clients
- The host entered `StartNewLevelSub()` and waited in network level sync while the client was still playing the old timeline
- After about 15 seconds, level sync continued with the client marked disconnected
- The new pickup trailer exposed the failure in the attached run, but the pickup and thief state were not responsible for the disconnect

## Fix

- Queue and transmit the rewind snapshot before changing host state
- Use the existing buffer-ready and apply-ready transfer phases as a restore barrier
- Have clients acknowledge apply immediately before entering restore
- Restore the host only after all required clients have entered the apply phase
- Keep single-player-host coop rewind immediate when no clients are connected

## Verification

- Scoped code-quality pass completed successfully
- Android `:app:assembleDebug` completed successfully for arm64-v8a, armeabi-v7a, and x86_64
- Windows D1 and D2 CMake builds completed successfully
- D1 native CTest suite passed: 24 of 24
- D2 native CTest suite passed: 28 of 28
- New transfer policy test is registered in both D1 and D2 CTest suites
- `git diff --check` passed

## Follow-up: rewind waiting status

- [x] Confirm the existing client-facing rewind transfer message
- [x] Show `Waiting to rewind` to the host after the rewind transfer is queued
- [x] Re-run scoped code quality and the Android debug build
