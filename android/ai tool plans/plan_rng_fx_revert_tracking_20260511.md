# RNG FX revert tracking 2026-05-11

## Goal
- continue the `_fx()` ownership survey without repeating already-reverted moves
- reconcile stale survey notes with the current D1/D2 code
- record which candidates were moved to `_fx()`, which were reverted, and why

## Steps
- [completed] update stale survey notes for `check_collision_delayfunc_exec()` and the conflicting AI sound-timer note
- [completed] confirm additional live `_fx()` sites remain effect-only after code-path reads
- [completed] capture reverted `_fx()` cases and reasons in the main survey record
- [completed] run focused validation on the note cleanup by searching for the removed stale claims

## Notes
- current confirmed reverted case: `check_collision_delayfunc_exec()` in `d1/main/collide.c` and `d2/main/collide.c`
- resolved note: `next_misc_sound_time` remains FX-owned because `compute_vis_and_vec` uses it only to throttle chatter playback and rewrite the timer itself
- additional confirmed FX-owned case this tranche: D2 `Next_seismic_sound_time` in `d2/main/weapon.c` only jitters looping rumble cadence after the SIM-owned disturbance/shake path has already determined gameplay state
- additional confirmed FX-owned note cleanup this tranche: `Fusion_next_sound_time` in `d1/main/game.c` and `d2/main/game.c` remains sound-cadence-only and stays on `d_rand_fx()`
- additional confirmed FX-owned fireball-helper case this tranche: `make_random_vector_fx()`, `create_small_fireball_on_object()`, and `create_vclip_on_object()` in `d1/main/object.c` and `d2/main/object.c` stay on `d_rand_fx()` because they only decorate already-dead or already-exploding objects, and the resulting fireballs are non-colliding expendable effects
