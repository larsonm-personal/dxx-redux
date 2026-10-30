# XL seismic gameplay and mine-shake actions

Next increment after owned mission sound banks. Implement the XL behavior in
`android/XL_compat`, retaining Redux as the alternate runtime path. This is a
gameplay and persistence change, not just a new trigger dispatch or audio cue.

## Published draft status

Branch `xl_more_75_seismic_gameplay` is a work-in-progress checkpoint for CI,
stacked on `xl_more_74_sound_banks`. The source model passes 23,100 comparisons
against the pinned XL implementation. Initial runtime, frame and persistence
hooks compile in the Windows headless engine.

This does not qualify seismic gameplay. Wall/object type-36 dispatch and
admission, position-bearing Earthshaker call sites, runtime physics/audio
controls, disk/rewind continuation and Android qualification remain unfinished.
The existing seismic audio controls still target the preceding implementation
and need to be extended for the new physics path. No mission blocker is removed
and no campaign playability claim is added by this draft.

## Grounded requirements

Pinned source remains `arbruijn/d2x-xl` revision
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. Consumed `weapons/seismic.cpp` hash:
`2c0e2c45cfef16b7713d63ff4a92368720733405613af929867ec433dd49bacc`.
`CTrigger::DoShakeMine` is type 36 in the pinned trigger definition.

- The action clamps negative strength to zero and uses ten seconds for negative
  authored time. Positive time is integer seconds, not fixed-point seconds.
- XL starts an eligible disturbance immediately and consumes its pending
  duration once. Redux chooses starts randomly and retains a repeat duration.
- XL's active disturbance calls `RockPlayerShip` with the raw absolute fixed
  time distance from the midpoint; Redux divides that value before clamping.
  Preserve actual source results rather than normalizing this difference away.
- Source rocking uses floating scale and FRound, applies player and companion
  rotational velocity, and publishes a tremor magnitude used by other consumers.
- Earthshaker events share this behavior, with four event slots and a
  distance-dependent scale beyond 200 units in the ordinary XL profile.
  The source also has a 40 Hz update option. Define and verify the selected
  simulation cadence, including paused and restored state.
- Existing shared audio handles the rumble channel, volume cadence and lifetime
  recovery, but existing seismic-audio tests do not qualify XL rocking physics.

The unchanged Pumo inventory contains two wall actions in `tulev1.rl2`, records
63 and 66: type 36, flags 3, value 196608 (strength 3), time 3, no targets.
These are representative original parameters, not permission to bypass Pumo's
other blockers or claim that the level is playable.

## Implementation and acceptance

1. Execute the actual source action, disturbance, rocking and Earthshaker
   functions with bounded clock, vector, random-draw and audio adapters. Compare
   normal, negative/default, zero, boundary, overlapping and slot-pressure cases.
   Keep simulation draws distinct from audio-only draws under the existing RNG
   policy and document any deliberate source correction.
2. Add XL-owned pending/active disturbance and detonation state. Route XL frames
   and position-bearing Earthshaker events to that owner with narrow D2 hooks;
   leave the Redux implementation intact. Inspect all four current native
   Earthshaker call sites in `collide.c` and `fireball.c`.
3. Connect wall and attached type-36 actions through the existing scheduler and
   source flag handling. Validate authored values and any unsupported range
   before admission. Cover retriggering, disabling, queued changes, expiry and
   object lifetime changes without broad mission-specific exceptions.
4. Save and restore the complete state, including scales, pending duration,
   active timing and update phase. Validate before mutation, preserve shared
   rumble ownership, and clear XL state on level and stock transitions. Do not
   infer complete persistence from the existing native timestamp-only state.
5. Use controlled native and Android events to compare actual player/companion
   physics, dependent magnitude and mixed PCM. Verify disk/rewind continuation,
   pause, level changes and unchanged Redux behavior. Extend existing owners
   where coverage fits; register any new top-level integration and run both
   Android catalog checks. No autonomous navigation.
6. Refresh original requirement evidence only after production controls pass.
   Removing two inventory rejections is not campaign qualification. Publish the
   tested increment and verify CI, retaining explicit remaining limits.
