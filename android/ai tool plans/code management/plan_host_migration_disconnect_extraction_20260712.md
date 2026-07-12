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
- [in progress] Run scoped formatting, focused fixtures, Windows D1/D2 builds, and Android native builds
- [completed] Record inherited-file line reduction and any remaining two-emulator validation

## Evidence

- The shared policy covers ordinary peer disconnect, local and remote host election, lowest-slot determinism, waiting/disconnected exclusion, stale host state, no-survivor fallback, and complete object-owner reset.
- `multi_disconnect_player()` now delegates the duplicated election, rewind reset, object ownership reset, powerup recount, migration metadata write, and Kotlin notification sequence to `coop_host_migration_handle_disconnect()`.
- D2 retains `escort_transfer_ownership_on_disconnect()` after the common host transition. The existing D2 escort-owner policy fixture still passes.
- `d1/main/multi.c` moved from `+490/-27` versus `main` to `+436/-27`; `d2/main/multi.c` moved from `+551/-30` to `+495/-30`. The tranche removes 110 inherited-file additions combined.
- Scoped code quality passed for all new shared policy/runtime files, the host fixture, and this plan. `git diff --check` passes apart from existing CRLF conversion notices.
- `run-windows-build.ps1 -Target both` passed. Both `test_coop_host_migration_policy.exe` binaries and D2 `test_escort_owner_policy.exe` pass.
- The campaign-wide combined Android build is pending after the concurrent H04/H05 edits settle.
- Remaining device acceptance is a two-emulator D2 coop cycle: disconnect the original host, verify the survivor owns host authority and Guide-Bot transfer, rejoin the former host, verify object sync and sustained PDATA, then repeat one host swap. The pure transition invariants no longer require an emulator.
