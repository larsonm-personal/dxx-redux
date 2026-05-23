# Level 9 replay analysis 2026-05-11 091552

## Goal
- determine whether `d2_descent2_level9_20260511_091552.dximdemo` exposes fresh game-engine nondeterminism or one of the already-known level 9 replay failure classes
- use the bundled result files and rng trace before adding more instrumentation
- confirm whether current source and host build still reproduce the failure after the later Gauss spread-order fix

## Steps
- [completed] locate the replay artifact bundle and inspect the result and rng-trace files for the first divergence signal
- [completed] compare the new failure against the recent level 9 investigations to decide whether it matches an existing root-cause class
- [completed] inspect the nearest owning code path for the first divergence and form one falsifiable hypothesis
- [completed] identify the Gauss spread argument-order hazard and land the D1/D2 fix in `laser.c`
- [completed] rerun the failing headless demo with the current host build to confirm the stale failure artifacts are gone
- [completed] reopen the next local divergence from fresh result files and reduce it to a live-only robot-contact fireball create at frame 99 / gt 914883
- [completed] align replay state comparison with the `_fx()` policy so collision-delay visual fireballs do not fail single-player determinism checks
- [completed] confirm the collision-delay jitter gate is sim-relevant because it also gates explosion object creation and allocator order in `collide.c`
- [completed] validate the corrected D2 code against regenerated level-9 recording `d2_descent2_level9_20260511_091859.dximdemo`

## Notes
- target demo: `android/regression_demos/d2_descent2_level9_20260511_091552.dximdemo`
- user report: failure is minor player position/orientation drift only, in a lava level, with possible Gauss-versus-lava or player-rock interaction nearby
- likely evidence sources: sibling `rngtrace` file, headless runtime wrapper result files, and recent level 9 plan notes from `2026-05-10`
- the earlier Gauss/Vulcan spread fix is still valid, but the next surviving mismatch after moving `check_collision_delayfunc_exec()` to `d_rand_fx()` is now a trace-policy issue rather than fresh sim RNG drift
- live replay creates `slot=187 type=1 id=1 sig=22319 seg=437` at `frame=99 gt=914883` with `ctx_obj=183 ctx_type=2 ctx_id=40 ctx_seg=437 ctx_control=1`; the recorded demo at the same `gt` has only the weapon delete and no matching create
- the extra object is a robot-contact collision visual from a path now driven by `_fx()` randomness, while the current state-trace comparer still treats raw allocator/fireball bookkeeping as deterministic simulation state
- `check_collision_delayfunc_exec()` cannot stay on `_fx()` because the same gate also controls `object_create_explosion(...)` in the player/robot collision path; moving its jitter off sim perturbs object slot order and eventually frame state
- after restoring `check_collision_delayfunc_exec()` to `d_rand()` in D1/D2, stale demo `d2_descent2_level9_20260511_091552.dximdemo` still remains a pre-fix artifact, but regenerated level-9 demo `d2_descent2_level9_20260511_091859.dximdemo` now replays with `RESULT: PASS`