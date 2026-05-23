# Thumbnail Cleanup Follow-Up

## Goal

Reduce the new D1 and D2 Android save-thumbnail duplication without changing the validated minimize-autosave behavior.

## Local hypothesis

- The newly added runtime thumbnail cache logic in `d1/main/state.c` and `d2/main/state.c` is duplicated scaffolding around the existing shared Android save-metadata abstraction.
- A tiny shared cache helper in `android_save_meta` can preserve the same trailer data flow while shrinking the D1/D2 diff and removing some local warning noise.

## Cheap check

- Move the cache storage and cache-to-params wiring into `android/app/src/main/cpp/shared/android_save_meta.*`.
- Rebuild Android and rerun the existing HOME-background autosave check, confirming the newest `auto_minimize` candidate still reports `has_thumbnail=true`.

## Steps

- [x] Centralize the duplicated PHYSFS Android save-metadata reader in shared save-metadata code
- [x] Trim nearby warning noise in the touched save path
- [x] Rebuild Android debug
- [x] Rerun a focused autosave-resume validation

## Outcome

- The duplicated `state_read_android_save_meta()` helper was removed from both `d1/main/state.c` and `d2/main/state.c` in favor of a single shared `android_save_meta_read_physfs()` helper.
- The touched save slice is cleaner: D1 no longer carries the unused `state_save_old_game()` local, and D2 no longer declares `gl_draw_buffer` on OGLES builds where it cannot be used.
- Android debug builds still pass after the cleanup, and the D2 autosave-resume flow still reaches the launcher assertions with the expected `auto_exit` resume candidate.