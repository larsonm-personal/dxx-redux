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
- `d1/main/collide.c` `check_collision_delayfunc_exec` line 91 and `d2/main/collide.c` line 283 are effect-only and now use `d_rand_fx()`. This helper jitters collision sound and impact-vclip cadence, and it belongs to the presentation stream even though old replay traces previously consumed the SIM draw here
- `d1/main/object.c` line 1642 and `d2/main/object.c` line 1955 now use `d_rand_fx()` for the dead-player small-fireball chance. The effect is a cosmetic post-death fireball on the ghosted player object, so the long-term stream ownership is FX even though old recorded demos may diverge until their baselines are regenerated or replay explicitly ignores effect-stream drift
- `d1/main/cntrlcen.c` line 124 and `d2/main/cntrlcen.c` line 142 now use `d_rand_fx()` for the dead-reactor burn-fireball cadence. The fireball helper is already FX-owned internally, so the gate roll belongs on FX too
- `d1/main/game.c` line 1433 and `d2/main/game.c` line 1958 now use `d_rand_fx()` for `Fusion_next_sound_time`. This is just the cadence of the warmup hum and overcharge sample, not fusion firing, damage, recoil, or bump physics
- `d1/main/cntrlcen.c` lines 152-153, `d2/main/cntrlcen.c` lines 182-183, `d1/main/game.c` lines 1302-1303, and `d2/main/game.c` lines 1593-1594 stay on SIM. Those rolls write into `ConsoleObject->mtype.phys_info.rotvel`, and `physics.c` integrates `rotvel` into live object orientation each frame, so these are gameplay-facing ship-motion changes rather than presentation-only jitter
- `d1/main/newdemo.c` line 3925 and `d2/main/newdemo.c` line 4447 now use `d_rand_fx()` for autoplay demo-file selection. This is menu/file choice only and has no business spending simulation RNG
- Debris creation randomness in `d1/main/fireball.c` and `d2/main/fireball.c` stays on SIM. `OBJ_DEBRIS` is a real physics object and `collide_weapon_and_debris` lets player weapons hit and explode debris, so its motion and lifetime still influence gameplay outcomes
- Validation for the long-term version was `run-windows-build.ps1 -Target d1` and `run-windows-build.ps1 -Target d2`, both passing. Old input-demo baselines are expected to drift because they previously encoded these effect-only SIM draws

## Single-Player RNG Recommendations
- `d1/main/ai.c` robot misc sound timers at lines 1281, 1309, and 1318, and `d2/main/ai2.c` at lines 1466, 1495, and 1506: completed. These effect-only cadence rolls now use `d_rand_fx()` in D1 and D2
- `d1/main/collide.c` `check_collision_delayfunc_exec` line 91 and `d2/main/collide.c` line 283: completed. The collision effect jitter now uses `d_rand_fx()` in D1 and D2
- `d1/main/object.c` line 1642 and `d2/main/object.c` line 1955: completed. The dead-player cosmetic fireball chance now uses `d_rand_fx()` in D1 and D2
- `d1/main/cntrlcen.c` line 124 and `d2/main/cntrlcen.c` line 142: completed. The dead-reactor burn-fireball cadence now uses `d_rand_fx()` in D1 and D2
- `d1/main/game.c` line 1433 and `d2/main/game.c` line 1958: completed. The fusion warmup and overcharge sound cadence now uses `d_rand_fx()` in D1 and D2
- `d1/main/newdemo.c` line 3925 and `d2/main/newdemo.c` line 4447: completed. Random autoplay demo selection now uses `d_rand_fx()` in D1 and D2
- `d2/main/laser.c` line 471: keep on SIM for now. The comment says omega blob lifetime makes lighting more interesting, but the lifetime belongs to a weapon object and can affect existence, cleanup, and collision timing. The visual-only omega lighting flicker was already moved in `d2/main/lighting.c`
- `d1/main/ai.c`, `d2/main/ai.c`, `d2/main/ai2.c`, and `d2/main/escort.c` AI behavior rolls: keep on SIM. This includes awareness, aiming error, cloaked-player estimate, boss teleport/gating, thief behavior, guidebot/escort path choice, and path randomization. Recommendation is targeted probes or event logging when a replay diverges, not FX stream movement
- `d1/main/aipath.c` and `d2/main/aipath.c` path shuffles and random segment selection: keep on SIM. These choose actual robot paths and can alter future collision and AI state
- `d1/main/cntrlcen.c`, `d2/main/cntrlcen.c`, `d1/main/game.c`, `d2/main/game.c`, and `d2/main/weapon.c` player shake, fusion overcharge, and seismic disturbance rolls: keep on SIM where they alter player rotation, damage, or future damage cadence. The fusion sound timer and dead-reactor fireball cadence are now on FX, but the ship-motion and damage-affecting rolls remain simulation-owned
- `d1/main/fireball.c`, `d2/main/fireball.c`, and `d2/main/weapon.c` debris, explosion, powerup-drop, wall-fireball, and spit-powerup rolls: keep on SIM unless a specific caller is proven cosmetic. These create physical objects, choose velocities, choose drops, or set object lifetime. Recommendation is event/probe coverage around robot death, debris creation, and wall explosion creation if object-list drift continues
- `d2/main/collide.c` Gauss robot-spin and forcefield/player bump rolls: keep on SIM. They consume RNG on physics-affecting hit paths and are good candidates for durable replay probes if Gauss-heavy demos still diverge
- `d1/main/laser.c` and `d2/main/laser.c` weapon spread, speed variation, lifetime variation, fusion recoil, and homing target selection: keep on SIM. The known order hazards are now sequenced; remaining work here is probe coverage, not stream movement
- `d1/main/gameseq.c`, `d2/main/gameseq.c`, `d1/main/object.c`, `d2/main/object.c`, `d1/main/fireball.c`, and `d2/main/fireball.c` timer-seeded random spawn/drop paths reviewed in this pass are multiplayer or network setup paths, so they are deferred to the later multiplayer survey
- After the conversions above, the remaining single-player `d_rand()` sites reviewed in this pass are either clearly simulation-owned or mixed-confidence gameplay cases, not more clear FX-only wins

## FX Label Pass 2026-05-11
- [completed] annotate every existing `d_rand_fx()` caller with a short ownership reason in code
- [completed] build touched D1 and D2 host targets to confirm the label pass stayed clean
