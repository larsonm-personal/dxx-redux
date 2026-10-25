# No-damage semantic audit

Status: source evidence and partial native collision controls, not admitted.
Original Observatory still rejects its 26 function-13 segments. The developer
17/36 layout control explicitly removes those functions and cannot qualify them.

The native test now separately binds an owned copy of the original segment
properties to that layout after validating geometry. That binding is compiled
only into the headless descriptor test. Two guarded engine hooks suppress final
player damage and direct weapon-player collision effects. Paired controls check
shields, player/projectile bytes, palette and effect creation against ordinary
segments. The same projectile survives the protected impact, then causes damage
and is disposed when the player is in an ordinary segment. Invalid-source binding
preserves prior properties, multiplayer disables them, and stock loading clears
them. Firing hooks now also gate primary input/queued fire, fresh missile input,
bomb drops and local weapon creation. Native controls retain flare energy and
cooldown use, and queued missile ammo/cooldown use without creating projectiles.
Parent/muzzle segment controls verify that the parent determines refusal, and a
robot parent still creates its weapon. Ordinary primary/missile controls fire
and spend resources after leaving the region. Broader fusion timing, guided flight/persistence,
full wall traversal, general splash behavior, save/restore, presentation and matched reference execution remain
unqualified. The full property remains unsupported.

Wall hooks and synthetic native contact controls now check the hit segment
independently of the player's segment. Protected ordinary impacts skip damage
and awareness; protected force fields retain blue palette flash and exactly the
ordinary seeded rotational response without shield loss. Volatile contact
preserves lava/water classification, suppresses damage/red flash, skips idle
rotation and its RNG consumption, and retains thrust-dependent rotation.
These tests alter contact texture records on the checked layout and restore
them afterward. They do not qualify original geometry traversal, audible effects,
scrape motion, authored wall triggers or source-renderer palette parity.

Paired native radius-explosion controls now compare identical coordinates and
initial physics with the test property enabled/neutralized. Both cases retain
identical nonzero force/rotation; only the ordinary case loses shields and gets
the damage flash. A real flash-weapon definition additionally retains blinding
palette state and the same Flash_effect duration under protection. This verifies
the existing central damage hook through the explosion path without changing
the explosion implementation. It does not qualify every missile, smart-child
propagation, cross-segment occlusion or rendered/audible behavior.

The pinned `effects/fireball.cpp:284-320` explicitly applies flash-missile
blinding, linear force and rotational force before `ApplyDamageToPlayer`.
These must survive protection, unlike the direct weapon/player collision which
returns before its impact effects. The native radius controls above check that
distinction; they are source-derived controls, not execution of the reference.

Player/clutter and player/player hooks now return before bump response and the
sound gate. Native singleplayer dispatch controls compare unchanged object bytes,
shields, RNG and sound-gate timestamp under protection against ordinary cases
that bump both objects and enter the sound gate. Direct cross-type segment
controls check that clutter dispatch uses the player segment and player-pair
dispatch uses the first receiver's segment, as in the reference. This is not
multiplayer qualification or audible playback evidence.

Reference actor/clutter dispatch also handles robot/debris and player/debris.
Redux currently disables those collision pairs in its dispatch table. Those
broader debris semantics remain unqualified; the new hooks do not enable them.

Guided state controls now launch through ordinary weapon dispatch with an owned
guided weapon, place the player in a protected segment, and verify that fresh
fire retains the exact missile object, guidance, ammo and cooldown. Leaving the
region allows ordinary release without another shot. A second launch followed
by a preexisting queued firing event in the protected region releases guidance
and updates cooldown without spending ammo or changing the projectile. This
matches `controlweapon.cpp:820-826` and `fireweapon.cpp:892-906`: the fresh-input
gate is outside queued dispatch, and guided release precedes ammo processing.
Boundary placement and the pending queue are explicit fixture setup, not earned
flight or replay evidence. Guidance rendering, physics traversal and persistence
remain unqualified.

A full native gameplay-frame control covers an already charged fusion timer
expiring inside protection. It retains charge and queues a shot without energy
use there, then recharges for one frame on exit before emitting two bolts with
the retained charge multiplier. Both recharge and authored emission energy costs
are checked. This exposed a real port difference: Redux FireLaser replaced the
queue with zero on released input, whereas reference FireGun retains it
(`weapons/fireweapon.cpp:1041-1050`) and ChargeFusion processes it on exit.
An XL adapter now preserves a charged pending fusion queue on released input
only for detected no-damage profiles, with a negative control preserving classic
behavior when those properties are absent. It is inactive in multiplayer and
stores no separate state across level changes.

This is a controlled timer/placement case with robot AI suspended, not earned
boundary flight. Additional frames verify that held fresh input inside protection
does not charge or spend energy. Native manual weapon selection cancels charge,
queue and timer, with no shot resurrected on exit. That is a regression control,
not reference parity: `controlweapon.cpp:499-500` refunds stored fusion charge
and clears it before manual primary selection, even before same-weapon and
availability checks. Redux clears charge on changing primary weapon without a
refund. This reference selection policy is not gated by the XL-level flag or
no-damage property and remains a separate unported behavior; do not infer it is
supported from the no-damage controls. Ordinary charging/overcharge cadence,
other selection paths, save/restore and matched reference execution remain open.

Reference: `arbruijn/d2x-xl` commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`.
Paths and line numbers below refer to that source, available in
`temp/d2x-xl-reference` during this investigation.

## Encoding and scope

`main/segment.cpp`, `CSegment::Upgrade`, converts old combined function 13 to
ordinary function plus property `SEGMENT_PROP_NODAMAGE` (8). `ReadExtras` calls
this upgrade for level versions below 24. `include/segment.h`,
`HasNoDamageProp`, tests that property. It is not a bit in the serialized ambient
segment flags. Keep original inventory and a distinct runtime property map;
do not emulate this using player invulnerability or a modified stock special.

The complete search for `HasNoDamageProp` found the consumers below. These are
source-level expectations; no live reference comparison has been run for them.

## Firing and resources

| Reference consumer                                      | Behavior and required distinction                                                                                                                                       |
| ------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `weapons/controlweapon.cpp:198`, `AllowedToFireGun`     | Local console segment blocks the gun gate before timing checks                                                                                                          |
| `weapons/fireweapon.cpp:653`, `LocalPlayerFireGun`      | A second gate rejects queued primary fire before energy/ammo accounting                                                                                                 |
| `weapons/controlweapon.cpp:241`, `AllowedToFireMissile` | Segment check applies only when requested by its caller; normal input uses it                                                                                           |
| `weapons/controlweapon.cpp:844`, bomb input             | Clears drop-bomb count without spending bombs                                                                                                                           |
| `weapons/createweapon.cpp:278`, `CreateNewWeapon`       | Rejects creation for the local console parent, using the parent's segment rather than the muzzle segment; robot parents take a different branch and remain able to fire |

Do not generalize these gates to "blocked firing never consumes resources":

- Flare input has no segment gate. `AllowedToFireFlare` updates its cooldown;
  `CreateFlare` (`fireweapon.cpp:401`) subtracts difficulty-scaled energy before
  the creation gate refuses the projectile
- Normal missile input gates adding to the global firing count, but processes
  an already nonzero count outside that gate (`controlweapon.cpp:820-826`).
  `DoMissileFiring` (`fireweapon.cpp:892`) handles guided release first, then
  updates timing and decrements ammo before attempting creation. Thus an
  already queued burst entering the region can spend ammo without a projectile
- `main/game.cpp:1076-1081` gates `FireGun`, but calls `FusionBump` separately
  and then the independently gated `LocalPlayerFireGun`. Pending fusion charge,
  queued shots and exit from the region need explicit temporal controls;
  clearing all weapon state on entry would be an unproven behavioral change
- Creation also performs gun-damage/random checks before the segment gate.
  This audit does not establish full reference RNG parity

## Damage and collisions

All following locations are in `physics/collide.cpp`:

| Reference consumer                | Behavior and boundary                                                                                                                                                      |
| --------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `:1805`, `ApplyDamageToPlayer`    | Returns based on the damaged player's current segment before shield damage and subsequent bookkeeping                                                                      |
| `:1887`, `CollideWeaponAndPlayer` | Returns before impact effects, hit bookkeeping, collision response and weapon disposal; immunity alone would retain unwanted effects                                       |
| `:567`, player-wall collision     | Uses the hit segment; returns after force-field response or ordinary wall processing, before bump damage, ordinary impact sound and awareness                              |
| `:611`, `ApplyWallPhysics`        | Zeros wall/environment damage after determining physics type; retains the physics type and subsequent thrust-dependent rotation, rather than treating the wall as ordinary |
| `:1789`, `CollidePlayerAndPlayer` | Uses the receiver's segment and skips bump response/sound                                                                                                                  |
| `:2076`, `CollideActorAndClutter` | Uses the actor's segment and skips bump response/sound; dispatch also includes robot/debris, so this is not only a player-shields rule                                     |

`CollideRobotAndPlayer` and `ApplyDamageToRobot` have no corresponding property
gate. Do not disable robot collisions, stealing, energy-drain behavior, robot
weapons or robot damage as a shortcut. Shield damage routed through
`ApplyDamageToPlayer` remains suppressed, while other collision effects may
remain. Similarly, this audit does not prove that splash force, awareness or
sound preceding the final player-damage call should disappear.

## Qualification sequence before admission

1. Preserve function-13 identity separately from stock geometry conversion and
   bind it to the same owned, validated source as the other XL adapters. Verify
   save/restore, failed selection, stock recovery and multiplayer rejection
2. Build paired ordinary/no-damage controls in the native engine for primary,
   missile, bomb and flare input, asserting objects, energy/ammo, cooldowns and
   queued state. Include charged fusion and guided control crossing the boundary
3. Test incoming direct weapons, splash, player/robot/clutter collision,
   ordinary wall and force-field hits, volatile wall contact, and robot firing
   and damage. Assert velocity, projectile survival, shields, effects and
   awareness, not only player survival
4. Exercise both directions across a shared face: player inside/muzzle outside,
   player outside/muzzle inside, hit segment differing from player segment,
   incoming projectile across the boundary, and pending fire on entry/exit.
   Run matched reference cases before claiming these source-derived details as
   verified semantics; record any deliberate deviations explicitly
5. Integrate the original Observatory only after these controls, independent
   descriptor/resource transaction qualification and original-level gameplay,
   lighting and presentation checks. Do not promote the neutralized layout
   fixture into evidence of original-level support

Fusion timing also uses `weapons/fusion.cpp`, SHA256
`452ffc572b8f397fd439a07b6c1328e068360cd413565149abad8c016d7b2700`.

## Reference file pins

| File                        | SHA256                                                             |
| --------------------------- | ------------------------------------------------------------------ |
| `weapons/controlweapon.cpp` | `1caa0d8a33f755260693a6083ac973593bf0533f2063815de72e66504d75d4b6` |
| `weapons/fireweapon.cpp`    | `37ae81f686e063ca7102bb9154368e67626e843ae898d82e8583167d31dbaac8` |
| `weapons/createweapon.cpp`  | `ba5b11cbed8606ef260cb100af15cd8e39c4a16ef79866ac5113135d3559ccdd` |
| `physics/collide.cpp`       | `d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9` |
| `effects/fireball.cpp`      | `e91c43142cbb1f85c0274aac19e97c95d62e95bc5aa31c1d2537db2113387bf4` |

Segment/header and main game pins are recorded in the adjacent
[segment flag audit](segment-flags-17-36.md).
