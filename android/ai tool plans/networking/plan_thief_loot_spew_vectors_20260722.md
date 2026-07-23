# Thief Loot Spew Vector Fix

## Goal

Restore distinct launch directions for individual stolen items dropped when a multiplayer thief explodes, while preserving synchronized item creation and the existing branch protocol version.

## Plan

- [done] Trace the thief explosion and multiplayer powerup creation paths and identify why all stolen items reuse one trajectory
- [done] Transmit and apply the authoritative per-item velocity through D2 `MULTI_CREATE_POWERUP`
- [done] Add focused Android send/receive velocity diagnostics for live regression verification
- [done] Run scoped code quality, the D2 Windows build, the existing thief network policy test, and the Android debug build
- [pending] Verify the visual spread and matching per-object velocities in a live two-client coop session

## Constraints

- Keep `MULTI_PROTO_VERSION` unchanged
- Preserve authoritative multiplayer object creation and cross-peer determinism
- Preserve unrelated worktree changes
- D1 has no thief, so a D2-only gameplay fix is expected

## Finding

The multiplayer thief-drop path still called the original `drop_powerup()`, which generated a distinct random velocity for each item. It then zeroed every created object's velocity before sending `MULTI_CREATE_POWERUP`. The receiver also unconditionally zeroed velocity because that packet previously contained only position. Consequently every stolen powerup started at the thief's exact death position with zero motion.

## Implemented Fix

- Keep each authoritative `drop_powerup()` result's generated velocity
- Extend the existing branch's D2 `MULTI_CREATE_POWERUP` payload with that velocity
- Apply the transmitted velocity to the mapped powerup on receiving peers
- Continue sending zero velocity for the unrelated stationary `maybe_drop_net_powerup()` path
- Log thief-drop and received-powerup velocity components on Android
- Keep `MULTI_PROTO_VERSION` unchanged at `30014`

## Validation

- `run-windows-build.ps1 -Target d2`: passed
- `buildd2\maths\test_thief_network_policy.exe`: passed
- Android `:app:assembleDebug` with JDK 21: passed
- Scoped code quality: passed
- `git diff --check`: passed with only CRLF conversion warnings
