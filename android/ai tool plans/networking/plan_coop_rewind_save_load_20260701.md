# Coop Rewind Save/Load Plan

## Goal
Make a non-host rewind in coop reload through the live coop save/load path so the host and clients stay connected when possible.

## Steps
- [x] Trace rewind trigger handling for host and client.
- [x] Trace coop multiplayer save/load packet flow and ownership rules.
- [x] Design a minimal protocol/control-flow change that keeps the session alive.
- [x] Implement matching D1 and D2 changes where applicable.
- [x] Add or update a focused regression/integration check if practical.
- [x] Run scoped formatting and an appropriate build or test.

## Notes
- Prefer existing coop save/load mechanics over a separate reconnect path.
- Keep D1 and D2 changes aligned unless investigation proves one game does not share this code path.
- Non-host rewind now remains a request to the host. The host selects and restores from its own memory rewind snapshot, then broadcasts that host-owned save buffer to clients in reliable begin/chunk/apply packets.
- Coop clients no longer capture their own rewind snapshots; they only restore the host-provided buffer for this flow.
- The Android rewind save-transfer implementation was factored out of both 1996-era `multi.c` files into the shared `multi_save_transfer.c` source, leaving only packet dispatch hooks in D1/D2 `multi.c`.
- Checks run: targeted `run-code-quality.ps1 -Fix`, native host builds/CTest for D1 and D2, and `:app:externalNativeBuildDebug` with JDK 21.
