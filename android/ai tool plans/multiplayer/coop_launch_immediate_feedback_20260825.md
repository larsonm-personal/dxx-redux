# Co-op launch immediate feedback

## Goal

Show launcher loading feedback immediately when starting a co-op game and remove any avoidable silent launch delay.

## Plan

- [x] Trace and time the co-op action from the lobby callback through launch preflight and metadata handoff
- [x] Start shared launcher preparation state at the first committed co-op launch action
- [x] Keep failures visible and reset preparation state on every blocked launch path
- [x] Add focused regression and introspection coverage
- [x] Run scoped code quality, focused and full unit tests, APK assembly, and co-op integration coverage

## Notes

- Preserve unrelated working-tree changes from other active tasks.
- Removed the LAN host-side wait for two 200 ms START retries; client retries continue in the background.
- Multiplayer launch preflight now runs on Dispatchers.IO while the shared launcher preparation dialog is active.
- Verification passed: scoped code quality, full debug unit tests, debug APK assembly, automation catalog validation, and `test_coop_launch_feedback.jsonc` on the emulator.
