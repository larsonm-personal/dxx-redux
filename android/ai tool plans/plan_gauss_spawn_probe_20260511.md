# Gauss spawn probe 2026-05-11

## Goal
- capture the first divergent D2 level 9 Gauss shot with replay probe logs at creation and first lifetime steps
- keep the change limited to replay debug instrumentation so normal builds and older findings stay intact
- use the rerun to decide whether divergence starts at spawn inputs or in the first movement and collision step

## Steps
- [completed] inspect the existing replay probe gates and current frame-362 to frame-367 evidence
- [completed] widen the replay probe window that gates player shot creation and early weapon lifetime logging for the first divergent Gauss shot
- [completed] rerun the failing replay with replay debug logging and verify the new spawn and early lifetime lines appear
- [completed] compare the new probe output against the recorded durable events and summarize the narrowed nondeterminism point
- [completed] route replay-time spread and firing-basis details into the actual probe log so the remaining create-time direction mismatch can be split between spread generation and basis application
- [completed] fix the Gauss and Vulcan spread call sites to evaluate `d_rand()` into temporaries before calling `Laser_player_fire_spread`
- [completed] rerun the failing D2 level 9 replay and verify the previous shield mismatch disappears
- [completed] audit Gauss collision side effects for blast-specific desync risk

## Notes
- the durable replay shot probe now shows the decisive mismatch point directly: before the fix, replay frame 361 logged `spreadr=317 spreadu=288` while the recorded demo logged `spreadr=288 spreadu=317`, even though both sides reached the same post-shot RNG state
- that combination identified unspecified C function-argument evaluation order as the root cause, because the inline `d_rand()` arguments in the Gauss and Vulcan `Laser_player_fire_spread` calls could be evaluated in opposite orders while still consuming the same two RNG values
- `Laser_create_new` keeps `parent_speed = 0` for Gauss and writes velocity as `direction * weapon_speed`, so the old create-time drift was upstream of create and is now resolved by the spread-order fix
- after switching the spread call sites to explicit temporaries, the failing D2 level 9 replay passes and the durable replay shot probe matches the recorded `spreadr` and `spreadu` ordering
- the Gauss collision audit did not turn up a special radial blast path as the main risk: the generic badass explosion path is only taken when `Weapon_info[weapon->id].damage_radius` is nonzero, and the explicit Gauss-specific side effect in collision code is the robot-spin branch that bumps `SKIP_AI_COUNT` and consumes three `d_rand()` calls on qualifying robot hits
- Gauss can still amplify desync after a shot path diverges through its weapon persistence rules and that robot-spin branch, but the level 9 replay failure fixed here was not a blast-radius issue