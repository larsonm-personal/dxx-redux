# Replay analysis 2026-05-09 level9 225113 level8 224859

## Goal
- classify the two new D2 desync demos by their first owner-level divergence
- use the newer replay owner logs, including generalized player-weapon lifetime and weapon-vs-robot FVI probes, before adding any more instrumentation
- determine whether either demo matches an existing failure class or opens a new owner path

## Steps
- [x] rerun both demos with state trace, rng trace, and replay debug log enabled
- [x] compare recorded vs replay rng traces to locate each first behavioral mismatch
- [x] inspect durable owner logs around the first mismatch frame for each demo
- [x] summarize whether the two demos collapse to an existing root cause or need a new owner hop

## Notes
- target demos:
  - `android/regression_demos/d2_descent2_level9_20260509_225113.dximdemo`
  - `android/regression_demos/d2_descent2_level8_20260509_224859.dximdemo`
- the latest probe additions now persist:
  - player-owned `weapon_life` replay breadcrumbs for non-flare weapons during collision-probe runs
  - `fvi_weapon_robot_check` replay probe lines in `ai_schedule_probe.log`
  - `probe_fvi_weapon_robot` recorder frame events for future redone demos
- replay reruns and rng compares:
  - level 9 demo `225113`: first rng semantic mismatch is frame `187` / `gt=608174` / `call_count=55826`; expected `d2/main/ai.c:create_awareness_event`, actual `d2/main/ai.c:do_ai_frame` on obj `173` sig `20711` id `42`
  - level 8 demo `224859`: first rng semantic mismatch is frame `202` / `gt=7657225` / `call_count=30267`; expected `d2/main/physics.c:phys_apply_rot`, actual `d2/main/ai.c:do_ai_frame` on obj `84` sig `13978` id `46`
- classification:
  - level 9 `225113` matches the existing player-weapon interaction timing family rather than a thief-only AI bug. In replay, player shot obj `5` stays alive through frame `187`; obj `173` then spends `call_count=55826` on the near-player awareness roll inside `do_ai_frame`, and only in frame `188` does obj `5` hit wall `132` and run `collide_weapon_wall` awareness at `calls=55828->55829`. The recorded rng order shows that same awareness call at frame `187`, so the first owner split is a one-frame-late player-shot wall impact that delays `create_awareness_event` and hands its rng slot to obj `173` AI.
  - level 8 `224859` is not an extra AI consumer. Both sides use the same frame-`202` rng values, but replay orders them as obj `84` agitation-path trigger roll first, then the player shot obj `123` robot-hit `phys_apply_rot` pair on robots `146` and `194`; the recording orders the two `phys_apply_rot` calls first and obj `84`'s `do_ai_frame` roll third. This is a same-frame ordering swap between player-shot collision physics and unrelated AI scheduling, not a new weapon class or gauss-specific path.
- likely next owner hop if we need more than classification:
  - level 9: instrument the player-weapon wall-impact timing and movement order for non-homing player shots one frame earlier, since the delayed wall impact is the first owner-visible difference
  - level 8: instrument same-frame object processing order around player shot obj `123`, its hit targets `146`/`194`, and obj `84`, since the first split is the order of `phys_apply_rot` versus the agitation-path `d_rand()` in `do_ai_frame`