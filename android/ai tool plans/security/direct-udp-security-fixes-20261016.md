# Direct UDP security fixes, 2026-10-16

## Objective

Implement and verify the fixes identified in `android/security_scan_direct_udp_20261016.md` for LAN and direct-IP UDP multiplayer in both games. Matchmaking services remain out of scope, but every packet accepted by the ordinary game socket remains in scope.

## Work order

- [x] P0 outer packet lengths and fail-closed dispatch
- [x] P0 PONG, proxy, P2P route, ACK, and MDATA identity/index safety
- [x] P0 observer queue and observer update bounds/authority
- [x] P0 semantic validation before MDATA forwarding
- [x] P0 transactional object-sync validation
- [x] P1 game-info, sync, and sequence semantic/string validation
- [x] P1 discovery reply rate limits, receive budgets, and sampled rejection logging
- [x] Add malformed-packet unit/integration coverage
- [x] Format, build D1/D2, and run relevant tests
- [x] Update the finding ledger with fix and verification evidence

## Placement

Prefer shared validation under `android/app/src/main/cpp/shared/` with minimal D1/D2 hooks. Do not add shared sources to the original source-variable lists in `d1/main/CMakeLists.txt` or `d2/main/CMakeLists.txt`; if a compiled shared source is needed, register it in the existing Android-specific `target_sources` area.

## Verification

- Scoped code quality passed
- Windows host builds passed for D1 and D2
- `test_udp_security_validation` passed for D1 and D2
- Android `externalNativeBuildDebug` passed for all configured ABIs after the final command-validator changes

No shared implementation source was added, and neither original D1/D2 source list was changed. The only CMake change registers the malformed-packet test in `android/tests/CMakeLists.txt`.
