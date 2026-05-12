# RNG Discipline Survey 2026-05-10

## Goal
Find and fix replay nondeterminism risks around game-engine RNG usage, especially call-argument order hazards and RNG that belongs in recorded `_fx()` wrappers

## Survey Lanes
- [completed] identify RNG calls used inside function arguments where C does not guarantee evaluation order
- [completed] identify recorder and replay paths where random choices should be captured in non-engine `_fx()` event helpers instead of recomputed during playback
- [completed] identify adjacent RNG lint patterns, including macro arguments, condition expressions with RNG side effects, and multi-draw call sites that need stable sequencing

## Validation Plan
- [completed] patch only clear local hazards found by the survey
- [completed] build affected D1 or D2 host targets
- [completed] rerun at least one focused replay if a D2 gameplay path changes

## Findings
- Fixed the remaining plain SIM-stream multi-draw argument-order hazard found by the scan: `object_create_debris` in both D1 and D2 passed three `d_rand()` expressions directly to `vm_vec_make`, so x, y, and z rotation velocity assignment depended on compiler argument evaluation order
- Fixed D2 omega-blob lighting flicker to use `d_rand_fx()` instead of `d_rand()`, since this roll is visual lighting in render calculation and should not spend the simulation RNG stream
- No remaining plain `d_rand()` statement with more than one draw was found after the debris fix
- Remaining `d_rand_fx()` multi-draw statements are condition plus body fireball effects, where the condition is sequenced before the body and no unsequenced argument order issue is present
- Wall-clock seeding of the SIM stream remains a larger follow-up class in multiplayer/session setup paths, including random player/spawn selection in `gameseq`, `object`, `fireball`, `multibot`, and `net_udp`; these need case-by-case handling because some values are network protocol or gameplay choices rather than visual effects
- Several fireball and debris SIM RNG uses look visual at first glance, but many create physical objects, powerup drops, pickup lifetimes, or wall explosion positions, so they were left on SIM pending more targeted replay evidence
- Validation passed with `run-windows-build.ps1 -Target d1`, `run-windows-build.ps1 -Target d2`, and the newest available D2 level 9 replay `d2_descent2_level9_20260510_232929.dximdemo`

## Single-Player Confirmation Results
- `d1/main/ai.c` robot misc sound timers at lines 1281, 1309, and 1318, and `d2/main/ai2.c` at lines 1466, 1495, and 1506 are effect-only and now use `d_rand_fx()`. These rolls only randomize angry or lurking sound cadence, so keeping them on SIM would preserve old-demo compatibility at the cost of the wrong long-term ownership boundary
- `d1/main/object.c` line 1642 and `d2/main/object.c` line 1955 now use `d_rand_fx()` for the dead-player small-fireball chance. The effect is a cosmetic post-death fireball on the ghosted player object, so the long-term stream ownership is FX even though old recorded demos may diverge until their baselines are regenerated or replay explicitly ignores effect-stream drift
- `d1/main/object.c` and `d2/main/object.c` helper randomness inside `make_random_vector_fx()`, `create_small_fireball_on_object()`, and `create_vclip_on_object()` stays on `d_rand_fx()`. These helpers only vary attached fireball/vclip placement, size, and crackle; the resulting `OBJ_FIREBALL` objects do not collide with gameplay actors, and the allocator already treats fireballs as expendable cleanup objects
- `d1/main/cntrlcen.c` line 124 and `d2/main/cntrlcen.c` line 142 now use `d_rand_fx()` for the dead-reactor burn-fireball cadence. The fireball helper is already FX-owned internally, so the gate roll belongs on FX too
- `d1/main/cntrlcen.c` line 337 and `d2/main/cntrlcen.c` line 397 stay on SIM. These rolls decide whether the reactor fires extra live shots after its main attack, so they directly change incoming projectile count and aim pressure
- `d1/main/game.c` line 1433 and `d2/main/game.c` line 1958 now use `d_rand_fx()` for `Fusion_next_sound_time`. This is just the cadence of the warmup hum and overcharge sample, not fusion firing, damage, recoil, or bump physics
- `d2/main/weapon.c` lines 989, 1072, and 1510 stay on `d_rand_fx()`. These draws only reschedule looping seismic rumble cadence after the simulation-owned shake/start rolls have already fired, so they do not alter damage, ship rotation, or the decision to begin a disturbance
- `d1/main/cntrlcen.c` lines 152-153, `d2/main/cntrlcen.c` lines 182-183, `d1/main/game.c` lines 1302-1303, and `d2/main/game.c` lines 1593-1594 stay on SIM. Those rolls write into `ConsoleObject->mtype.phys_info.rotvel`, and `physics.c` integrates `rotvel` into live object orientation each frame, so these are gameplay-facing ship-motion changes rather than presentation-only jitter
- `d1/main/newdemo.c` line 3925 and `d2/main/newdemo.c` line 4447 now use `d_rand_fx()` for autoplay demo-file selection. This is menu/file choice only and has no business spending simulation RNG
- `d2/main/collide.c` lines 1848 and 1856 now use `d_rand_fx()` for buddy hint delay and hint-text selection when the player hits an invulnerable boss spot. These rolls only affect guidebot message timing and wording, and the associated state is not used anywhere else in the game logic
- `d2/main/collide.c` line 2340 stays on SIM. This roll decides whether player death arms and spawns a real smart mine in `drop_player_eggs_remote()`, so it directly changes explosive object state in single player
- `d2/main/fireball.c` line 1387 stays on SIM. This roll decides whether the robot spawned from `drop_powerup(OBJ_ROBOT, ...)` also drops a real shield pickup, so it directly changes pickup existence
- `d2/main/fireball.c` lines 1413-1430 stay on SIM. These rolls decide whether shield or energy drops are suppressed in `object_create_egg()` when the player already has high shields or energy, so they directly change whether the pickup exists at all
- `d2/main/laser.c` line 867 stays on SIM. This roll applies `speedvar` to live projectile speed, so it directly changes weapon travel time and trajectory
- `d1/main/laser.c` line 417 and `d2/main/laser.c` line 894 stay on SIM. These rolls vary live flare lifetime, so they directly change how long the flare object remains in the world
- `d1/main/laser.c` line 1318 and `d2/main/laser.c` line 2328 stay on SIM. These rolls set live vulcan shot spread, so they directly change projectile paths and hit results
- `d1/main/laser.c` line 1502 and `d2/main/laser.c` line 2604 stay on SIM. These rolls choose which live target each smart child homes toward, so they directly change missile behavior and target pressure
- `d1/main/laser.c` lines 1370-1372 and 1606-1608, plus `d2/main/laser.c` lines 2379-2381 and 2816-2818, stay on SIM. These rolls feed `phys_apply_rot`, so they directly change live recoil rotation after firing fusion and mega-class weapons
- `d1/main/fuelcen.c` and `d2/main/fuelcen.c` stay on SIM. Their rolls set live matcen spawn timing and choose which robot type actually materializes, so they directly change when and what spawns into the level
- `d1/main/aipath.c`, `d2/main/aipath.c`, and the single-player pathing rolls in `d2/main/escort.c` stay on SIM. These rolls shuffle path traversal, perturb live path geometry, choose retreat and scram path lengths, and pick the thief's respawn segment, so they directly change live AI movement and routing
- `d1/main/ai.c`, `d2/main/ai2.c`, and the single-player thief-steal rolls in `d2/main/escort.c` stay on SIM. These rolls decide live firing suppression, aim leading and spread, boss spew and teleport targets, awareness and agitation changes, and whether the thief removes real inventory items
- `d1/main/game.c`, `d2/main/game.c`, `d1/main/gameseg.c`, `d2/main/gameseg.c`, and `d2/main/physics.c` stay on SIM. These rolls set live fusion overcharge damage, breadcrumb powerup lifetime, random placement points inside segments, and stochastic AI-physics integration, so they directly change gameplay state or simulation timing
- `d2/main/ai.c` stays on SIM. Its rolls decide live agitation path creation, death re-pathing, companion and thief flare refire delay, wake and search transitions, and awareness and agitation propagation
- Debris creation randomness in `d1/main/fireball.c` and `d2/main/fireball.c` stays on SIM. The random draws set `OBJ_DEBRIS` velocity, rotation, and lifetime on real physics objects, and `collide_weapon_and_debris` lets player weapons hit and explode debris, so these rolls directly affect object existence and collision timing
- Robot contained-drop randomness in `d1/main/fireball.c` and `d2/main/fireball.c` stays on SIM. These draws decide whether a destroyed robot drops contents and how many items `object_create_egg()` spawns, so they directly change pickup availability and later gameplay state
- Validation for the long-term version was `run-windows-build.ps1 -Target d1` and `run-windows-build.ps1 -Target d2`, both passing. Old input-demo baselines are expected to drift because they previously encoded these effect-only SIM draws
- Additional 2026-05-11 validation after the collision-delay revert: `run-windows-build.ps1 -Target d1` passed, and regenerated level-9 replay `android/regression_demos/d2_descent2_level9_20260511_091859.dximdemo` passed with full state-trace comparison
- Additional 2026-05-11 validation after the D2 buddy-hint `_fx()` move: `run-windows-build.ps1 -Target d2` passed, and `android/tests/run_input_demo_headless.ps1 -DemoPath android/regression_demos/d2_descent2_level9_20260511_091859.dximdemo -Game d2 -Mode accelerated -CompareStateTrace` returned `RESULT: PASS`

## Reverted `_fx()` Cases
- `d1/main/collide.c` `check_collision_delayfunc_exec()` line 97 and `d2/main/collide.c` line 289 were initially moved to `d_rand_fx()` and then reverted to `d_rand()`. The helper does jitter collision sound/explosion throttling, but the same gate also controls `object_create_explosion(...)` on live player/robot collisions, so `_fx()` jitter perturbs fireball allocation and later object-processing order. Treat this as simulation-owned unless the explosion-allocation side effect is structurally separated. Do not conflate this with the `object.c` attached fireball helpers, which only decorate already-exploding or dead objects and generate non-colliding expendable fireballs

## Single-Player RNG Recommendations
- `d1/main/ai.c` robot misc sound timers at lines 1281, 1309, and 1318, and `d2/main/ai2.c` at lines 1466, 1495, and 1506: completed. These effect-only cadence rolls now use `d_rand_fx()` in D1 and D2
- `d1/main/collide.c` `check_collision_delayfunc_exec()` line 97 and `d2/main/collide.c` line 289: reverted. This was an initial `_fx()` candidate, but it must stay on SIM because the same gate controls live fireball allocation in the player/robot collision path
- `d1/main/object.c` line 1642 and `d2/main/object.c` line 1955: completed. The dead-player cosmetic fireball chance now uses `d_rand_fx()` in D1 and D2
- `d1/main/object.c` and `d2/main/object.c` helper randomness inside `make_random_vector_fx()`, `create_small_fireball_on_object()`, and `create_vclip_on_object()`: completed. Keep these on `d_rand_fx()`; attached fireball/vclip placement and size stay effect-only, and the resulting fireballs are already non-colliding expendable objects rather than gameplay-affecting contact-path explosions
- `d1/main/cntrlcen.c` line 124 and `d2/main/cntrlcen.c` line 142: completed. The dead-reactor burn-fireball cadence now uses `d_rand_fx()` in D1 and D2
- `d1/main/cntrlcen.c` line 337 and `d2/main/cntrlcen.c` line 397: confirmed SIM-owned. These rolls decide whether the reactor emits extra live shots after the main attack, so they directly change projectile count and combat pressure
- `d1/main/game.c` line 1433 and `d2/main/game.c` line 1958: completed. The fusion warmup and overcharge sound cadence now uses `d_rand_fx()` in D1 and D2
- `d1/main/newdemo.c` line 3925 and `d2/main/newdemo.c` line 4447: completed. Random autoplay demo selection now uses `d_rand_fx()` in D1 and D2
- `d2/main/collide.c` lines 1848 and 1856: completed. Buddy hint delay and hint-text choice now use `d_rand_fx()` because they only drive guidebot messaging after invulnerable boss hits
- `d2/main/collide.c` line 2340: confirmed SIM-owned. This roll decides whether `drop_player_eggs_remote()` arms and spawns a real smart mine on player death in single player
- `d2/main/fireball.c` line 1387: confirmed SIM-owned. This roll decides whether a spawned robot also drops a real shield pickup
- `d2/main/fireball.c` lines 1413-1430: confirmed SIM-owned. These rolls decide whether shield or energy drops are suppressed in `object_create_egg()` based on the player’s current resources
- `d2/main/laser.c` line 867: confirmed SIM-owned. This roll applies weapon `speedvar` to live projectile speed
- `d1/main/laser.c` line 417 and `d2/main/laser.c` line 894: confirmed SIM-owned. These rolls vary live flare lifetime
- `d1/main/laser.c` line 1318 and `d2/main/laser.c` line 2328: confirmed SIM-owned. These rolls set live vulcan shot spread
- `d1/main/laser.c` line 1502 and `d2/main/laser.c` line 2604: confirmed SIM-owned. These rolls choose smart-child homing targets
- `d1/main/laser.c` lines 1370-1372 and 1606-1608, plus `d2/main/laser.c` lines 2379-2381 and 2816-2818: confirmed SIM-owned. These rolls feed `phys_apply_rot` for live recoil rotation
- `d1/main/fuelcen.c` and `d2/main/fuelcen.c`: confirmed SIM-owned. Their rolls set live matcen spawn timing and materialized robot type
- `d1/main/aipath.c`, `d2/main/aipath.c`, and the single-player pathing rolls in `d2/main/escort.c`: confirmed SIM-owned. Their rolls alter live AI route order, path geometry, retreat and scram path lengths, and thief respawn placement
- `d1/main/ai.c`, `d2/main/ai2.c`, and the single-player thief-steal rolls in `d2/main/escort.c`: confirmed SIM-owned. Their rolls alter live firing, aim, awareness and agitation, boss spawn and teleport choices, and real player inventory loss
- `d1/main/game.c`, `d2/main/game.c`, `d1/main/gameseg.c`, `d2/main/gameseg.c`, and `d2/main/physics.c`: confirmed SIM-owned. Their rolls alter live damage, object lifetime, placement inside segments, and stochastic AI-physics integration
- `d2/main/ai.c`: confirmed SIM-owned. Its rolls alter live agitation pathing, chase and wake transitions, flare refire delay, and awareness/agitation propagation
- `d2/main/weapon.c` lines 989, 1072, and 1510: completed. `Next_seismic_sound_time` stays on `d_rand_fx()` because it only jitters the looping rumble cadence after the SIM-owned disturbance/shake path has already decided the gameplay state
- `d2/main/laser.c` line 471: confirmed SIM-owned. The comment mentions lighting, but the random lifetime is assigned to real `OBJ_WEAPON` `OMEGA_ID` blobs; the last blob is explicitly positioned to cause damage, and the collision handlers special-case omega damage. The visual-only omega lighting flicker was already moved in `d2/main/lighting.c`, so the remaining lifetime draw stays on SIM
- `d1/main/ai.c`, `d2/main/ai.c`, `d2/main/ai2.c`, and `d2/main/escort.c` AI behavior rolls: keep on SIM. This includes awareness, aiming error, cloaked-player estimate, boss teleport/gating, thief behavior, guidebot/escort path choice, and path randomization. Recommendation is targeted probes or event logging when a replay diverges, not FX stream movement
- `d1/main/aipath.c` and `d2/main/aipath.c` path shuffles and random segment selection: keep on SIM. These choose actual robot paths and can alter future collision and AI state
- `d1/main/cntrlcen.c`, `d2/main/cntrlcen.c`, `d1/main/game.c`, `d2/main/game.c`, and `d2/main/weapon.c` player shake, fusion overcharge, and seismic disturbance rolls: keep on SIM where they alter player rotation, damage, or future damage cadence. The fusion sound timer and dead-reactor fireball cadence are now on FX, but the ship-motion and damage-affecting rolls remain simulation-owned
- `d1/main/fireball.c` and `d2/main/fireball.c` debris creation rolls: confirmed SIM-owned. They randomize `OBJ_DEBRIS` velocity, rotation, and lifetime on physical debris objects, and those debris objects can still be hit and exploded by player weapons via `collide_weapon_and_debris`
- `d1/main/fireball.c` and `d2/main/fireball.c` robot contained-drop rolls: confirmed SIM-owned. They decide whether destroyed robots spawn contained pickups and how many items `object_create_egg()` emits, so they directly change single-player resource availability
- `d1/main/fireball.c` and `d2/main/fireball.c` `drop_powerup()` rolls at lines 959-961 and 1019 in D1, and 1230-1232 and 1290 in D2: confirmed SIM-owned. They randomize the velocity and timed lifetime of real dropped `OBJ_POWERUP` objects, so they directly affect pickup trajectory and availability
- `d2/main/weapon.c` `spit_powerup()` rolls at lines 1229-1231 and 1282, plus the caller seed rolls at lines 1311 and 1461: confirmed SIM-owned. They seed and randomize the velocity and lifetime of real dropped `OBJ_POWERUP` objects for weapon-drop paths, so they directly affect pickup trajectory, availability, and lifetime
- `d1/main/fireball.c` and `d2/main/fireball.c` explodable-wall fireball position rolls at lines 1498-1499 and 1851-1852: confirmed SIM-owned. These randomize where wall-destruction explosions spawn on the wall face, and every fourth spawn uses `object_create_badass_explosion()` with nonzero damage, radius, and force
- `d1/main/fireball.c`, `d2/main/fireball.c`, and `d2/main/weapon.c` other explosion rolls: keep on SIM unless a specific caller is proven cosmetic. These create physical objects, choose velocities, or set object lifetime. Recommendation is event/probe coverage around robot death and wall explosion creation if object-list drift continues
- `d2/main/collide.c` Gauss robot-spin and forcefield/player bump rolls: keep on SIM. They consume RNG on physics-affecting hit paths and are good candidates for durable replay probes if Gauss-heavy demos still diverge
- `d1/main/laser.c` and `d2/main/laser.c` weapon spread, speed variation, lifetime variation, fusion recoil, and homing target selection: keep on SIM. The known order hazards are now sequenced; remaining work here is probe coverage, not stream movement
- `d1/main/gameseq.c`, `d2/main/gameseq.c`, `d1/main/object.c`, `d2/main/object.c`, `d1/main/fireball.c`, and `d2/main/fireball.c` timer-seeded random spawn/drop paths reviewed in this pass are multiplayer or network setup paths, so they are deferred to the later multiplayer survey
- After the conversions above, the remaining single-player `d_rand()` sites reviewed in this pass are either clearly simulation-owned or mixed-confidence gameplay cases, not more clear FX-only wins

## FX Label Pass 2026-05-11
- [completed] annotate every existing `d_rand_fx()` caller with a short ownership reason in code
- [completed] build touched D1 and D2 host targets to confirm the label pass stayed clean
