# Host migration and disconnect extraction

## Goal

Add deterministic coverage for the D1/D2 host-loss path, then reduce the duplicated inherited-file implementation by moving only game-neutral election and reset mechanics into shared code. Preserve D2-only Guide-Bot ownership transfer and all game-owned side effects.

## Constraints

- Preserve existing multiplayer packet layout and reconnect behavior
- Keep D1/D2 call sites small and stylistically consistent
- Do not touch Kconfig, newmenu, classic-demo, or input-demo policy work
- Preserve unrelated route-planner and metadata changes in the shared worktree
- Exercise host loss, ordinary client disconnect, local host adoption, and remote-host selection deterministically before extraction

## Plan

- [completed] Reconcile prior host-migration/rejoin plans with the live D1/D2 implementation
- [completed] Inventory exact D1/D2 divergence and identify a compact game-neutral boundary
- [completed] Add host-side fixtures for election and disconnect transition invariants
- [completed] Extract the common transition mechanics and retain local side effects
- [completed] Run scoped formatting, focused fixtures, Windows D1/D2 builds, and Android native builds
- [completed] Record inherited-file line reduction and two-emulator validation

## Evidence

- The shared policy covers ordinary peer disconnect, local and remote host election, lowest-slot determinism, waiting/disconnected exclusion, stale host state, no-survivor fallback, and complete object-owner reset.
- `multi_disconnect_player()` now delegates the duplicated election, rewind reset, object ownership reset, powerup recount, migration metadata write, and Kotlin notification sequence to `coop_host_migration_handle_disconnect()`.
- D2 retains `escort_transfer_ownership_on_disconnect()` after the common host transition. The existing D2 escort-owner policy fixture still passes.
- `d1/main/multi.c` moved from `+490/-27` versus `main` to `+436/-27`; `d2/main/multi.c` moved from `+551/-30` to `+495/-30`. The tranche removes 110 inherited-file additions combined.
- Scoped code quality passed for all new shared policy/runtime files, the host fixture, `game_introspect.cpp`, the LAN runner, and this plan. The PowerShell parser and PSScriptAnalyzer pass, and `git diff --check` passes apart from existing CRLF conversion notices.
- `run-windows-build.ps1 -Target both` passed. Both `test_coop_host_migration_policy.exe` binaries and D2 `test_escort_owner_policy.exe` pass.
- `gradlew.bat :app:assembleDebug --console=plain --no-daemon` passes for D1 and D2 on arm64-v8a, armeabi-v7a, and x86_64. The final installed APK SHA-256 was `FE1F7D5680E118EDF4A76433EFB12FF0CE27EBFDF5FD20315C6727907BC62A74`.
- Migrated-master transport no longer assumes slot 0: reliable MDATA/ACK, PDATA send/relay/authentication, queue timeout fanout, ping/endlevel/netgame fanout, proxy fallback/reset, sync abort/leave fanout, and observer routing use the elected master or explicitly skip the local slot. D1's Android-only full game-info/sync packet now carries the elected master slot, matching the existing D2 extension while leaving desktop packet layout unchanged.
- Debug introspection retains raw active-object counts and separately publishes the exact synchronized-object domain used by each engine's rejoin transfer. D1 covers powerups, players/ghosts, control centers, robots, and hostages; D2 also covers deployed proximity mines. Authority assertions use this stable synchronized signature instead of local transient objects.
- `test_lan.ps1 -SkipBuild -HostMigration -Game d2 -TimeoutSeconds 120` passes a full two-swap coop cycle: initial bidirectional PDATA, slot-1 election and object-owner reset, Guide-Bot authority transfer, former slot-0 rejoin through port 42425 with role/callsign/object/Guide-Bot parity and PDATA, slot-0 reelection, and reciprocal slot-1 rejoin with the same parity and traffic checks.
- `test_lan.ps1 -SkipBuild -HostMigration -Game d1 -TimeoutSeconds 120` passes the corresponding full two-swap cycle with both fresh rejoin process gates, role/callsign/synchronized-object parity, owner reset, and bidirectional PDATA. D1 omits only the D2-specific Guide-Bot checks.
- The runner now omits empty D1 mission extras, deletes stale introspection before launches and rejoins, treats a missing game process as fatal, accepts scalar or multi-line host diagnostics, and gives paired migration preparation the requested runtime timeout. These checks prevented stale launcher state from being mistaken for a successful rejoin.
