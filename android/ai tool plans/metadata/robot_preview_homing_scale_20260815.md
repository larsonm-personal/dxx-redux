# Robot Preview Homing and Scale Corrections

## Objective

Correct preview-space homing direction, regular-robot zoom consistency, and D1 projectile display scale for fireballs and mines.

## Plan

- [x] Capture preview introspection for Super Mech homing flight, both Class 1 Drillers, and representative D1 laser, fireball, and mine weapons.
- [x] Correct homing guidance so stage-right shots cannot reverse across the preview.
- [x] Make equivalent cloaked and uncloaked robot models use consistent visual scale.
- [x] Derive projectile render size from the weapon renderer's native sizing rules instead of a laser-oriented fallback.
- [x] Extend integration assertions for homing direction, robot display ratio, and projectile display size.
- [x] Run scoped quality checks, Android and Windows builds, CTest, and focused emulator tests.

## Findings

- Homing missiles steered toward one finite stage-right point, but only a radius-based hit test retired them. A near miss passed the point and then homed back across the screen.
- Homing projectiles now complete when they cross the target's stage-right plane. The test explicitly fires Super Mech weapon 21 toward that plane and rejects any positive-to-negative horizontal velocity reversal.
- D2 robots 5 and 19 use separate models with the same 5.2705-unit radius. Both Class 1 Driller variants now share a canonical 4/3 camera-distance adjustment, reducing their displayed radius ratio from 0.8654 to 0.6490 while keeping cloak variants matched.
- D1 fireball weapon 26 referenced vclip 54, which the preview incorrectly rejected against `Num_vclips` before drawing a three-pixel fallback particle. The engine uses the fixed `VCLIP_MAXNUM` table for these references, and the preview now does the same.
- D1 round fireball and mine vclips use a 1.5x preview-only display scale. Elongated laser sprites and polymodel laser weapons retain their native 1.0x scale.

## Validation

- Scoped code quality passed for the shared C++ and PowerShell integration test.
- Android debug APK built successfully for arm64-v8a, armeabi-v7a, and x86_64.
- Emulator checks passed for D2 Super Mech stage-right homing, D2 cloaked Class 1 Driller scale, D1 Heavy Driller fireball rendering, D1 Gopher mine rendering and two-mine retention, and D1 PTMC Defense laser native scale.
- Windows D1 and D2 builds passed. CTest passed all 33 D1 tests and all 40 D2 tests.
