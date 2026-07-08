# Coop Autosave Restore Existing Protocol Fix 2026-07-08

## Goal
- Reuse the existing multiplayer save/load protocol for Android coop autosave resume instead of substituting peer resync

## Steps
- [x] Compare current Android restore branches against upstream restore flow
- [x] Make Android coop multiplayer saves use save-set filenames
- [x] Broadcast host coop autosaves through `MULTI_SAVE_GAME`
- [x] Let peers process `MULTI_RESTORE_GAME` for autosave slots
- [x] Run focused code quality and build checks

## Notes
- Upstream `MULTI_RESTORE_GAME` always calls `multi_restore_game()` on peers
- The Android autosave resync shortcut bypassed that path and left clients on fresh level state
- Autosaves now use a unique game id again so stale peer slot files can fail the existing id check
- `android\run-code-quality.ps1 -Fix` passed on the edited paths
- Android object compiles passed for D1/D2 `multi.c` and `coop_save.c` on `arm64-v8a`, `armeabi-v7a`, and `x86_64`
- Full Windows D1/D2 builds are currently blocked by unrelated `GL_GEQUAL` compile errors in `d1/main/render.c` and `d2/main/render.c`
