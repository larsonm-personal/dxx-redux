# Robot Preview Zoom and Super Mech Projectile Fix

## Objective

Make robot preview scale stable and proportional across robots, and restore Super Mech's visible homing-missile attacks.

## Plan

- [x] Measure loaded model radii and effective camera distances for representative small, medium, large, boss, and reactor robots.
- [x] Correct the camera tier assignment so comparable robots share a stable reference distance and preserve relative size.
- [x] Trace Super Mech gun and secondary-weapon scheduling against the base D2 AI firing rules.
- [x] Fix secondary-weapon selection or projectile lifetime/off-screen handling as required.
- [x] Extend robot preview introspection and integration checks for proportional scale and Super Mech homing missiles.
- [x] Run scoped quality checks, native builds, Windows builds, CTest, and focused emulator tests.

## Findings

- The large-camera predicate treated every model even slightly larger than Super Mech as a boss-scale object, then placed it at the distance required by the largest boss. That discontinuity made nearly equal-sized robots appear radically different.
- Camera tiers now follow semantic data: boss-flagged robots and the D2 mini reactor use the large tier; all other robots use the Super Mech reference tier. Robots within a tier therefore retain their model-radius proportions.
- Base D2 Super Mech is robot 16 with weapon 21. The loaded weapon is a homing polymodel missile with a blob size of 10 units.
- The off-screen test used twice the weapon blob size as its near-camera boundary for every render type. Super Mech's missile was retired immediately at its gun point before its polymodel could render.
- Near-camera retirement now occurs only after the projectile center crosses the camera plane. Horizontal and vertical retirement still accounts for visual radius.

## Validation

- Scoped code quality and `git diff --check` passed.
- The updated D2 x86_64 Android native target built successfully with no preview-code warnings.
- A temporary aligned and debug-signed emulator APK containing that target passed signature verification.
- Base D2 Super Mech passed with normal camera tier, homing weapon 21, actual polymodel projectile rendering, orientation-dependent firing, and off-screen retirement.
- Base D2 Mega Hulk passed with the large boss camera tier.
- Base D2 Class 1 Driller passed with the normal camera tier and particle-projectile off-screen retirement.
- Windows D1 and D2 builds passed.
- CTest passed 33 of 33 D1 tests and 40 of 40 D2 tests.
- The full Android Gradle build was independently blocked by an unrelated in-progress D1 automation change that uses `Countdown_timer` without making `cntrlcen.h` available to the D1 compilation of `game_automate.cpp`.
