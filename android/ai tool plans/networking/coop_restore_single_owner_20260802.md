# Coop restore single-owner implementation

## Goal

Make the Host/Create dialog the sole owner of the coop restore choice. Default to
the newest full save, preserve the exact selected save through LAN and online
lobbies, and never silently replace it with another save.

## Plan

- [x] Replace lobby save reranking and mutation with a read-only restore summary
- [x] Keep the newest full save as the default Host/Create selection
- [x] Keep the restore choice and advertised level under the same Host/Create owner
- [x] Add focused unit tests for default selection, exact-slot preservation,
      explicit fresh starts, and checkpoints
- [x] Run scoped formatting, tests, and the required Windows build

## Notes

- Save compatibility is not required before release
- The lobby may display the selected restore but must not change it
- A missing or mismatched selected save must not fall back to a newer save
- The existing typed JSON handoff remains sufficient after removing all lobby
  writers, so no native save-format change was needed

## Verification

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- Focused multiplayer unit tests: passed
- Full `:app:testDebugUnitTest :app:assembleDebug`: passed
- `run-windows-build.ps1` for D1 and D2: passed
