# Fix: Coop Invulnerability After Save/Load

## Root Cause
`do_invulnerable_stuff()` in game.c skips `multi_send_ship_status()` when `FakingInvul==1`
(spawn invulnerability). In the Android coop port, `multi_do_ship_status()` directly assigns
player flags including INVULNERABLE. When spawn invuln expires locally but the ship_status
update is suppressed, the remote machine retains a stale INVULNERABLE flag permanently.

Unlike `do_cloak_stuff()` which iterates all players, `do_invulnerable_stuff()` only checks
`Player_num`, so the stale flag is never timer-cleared on the remote machine.

## Fix
Add `multi_send_ship_status()` when FakingInvul expires in coop mode.

### Files
- [x] d2/main/game.c - do_invulnerable_stuff: add ship_status send on FakingInvul expiry
- [x] d1/main/game.c - same fix
- [x] Build Android APK (assembleDebug) - passed
- [x] Run code quality linters - passed (clang-format, PSScriptAnalyzer, shellcheck all clean)
