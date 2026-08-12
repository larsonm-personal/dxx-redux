# Coop save death spew lifetime fix

## Goal

Keep player death-spew powerups immortal after loading a cooperative save when the cooperative quality-of-life setting is enabled.

## Plan

- [completed] Trace death-spew lifetime assignment, cooperative save serialization and restore, and the QoL setting lifetime
- [completed] Apply the smallest matching D1 and D2 fix while preserving normal timed powerups
- [completed] Add focused regression coverage where practical
- [completed] Run scoped formatting, relevant tests, and build verification
- [completed] Record verification results and mark this plan complete

## Notes

- Preserve the user's existing change in `android/outstanding_bugs.md`

## Results

- Added a shared cooperative-restore pass that reapplies `IMMORTAL_TIME` only to loaded `OBJ_POWERUP` objects carrying `OF_PLAYER_DROPPED` when `Netgame.PlayerSpewNoExpire` is enabled
- Called the pass from both D1 and D2 after cooperative metadata validation, so every peer applies the live session policy after loading the authoritative save
- Added paired restore-policy coverage to `android/tests/test_state_persistence_contracts.py`
- Scoped code-quality checks passed
- `python -m unittest android.tests.test_state_persistence_contracts` passed all 8 tests
- `:app:externalNativeBuildDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- `run-windows-build.ps1 -Target both` was attempted, but the repository-wide build is independently blocked in the `test_android_save_set` target because the current `android_save_set.c` uses `PATH_MAX`, which MSVC does not define
