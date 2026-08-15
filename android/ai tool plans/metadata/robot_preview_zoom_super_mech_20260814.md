# Robot Preview Zoom and Super Mech Projectile Fix

## Objective

Make robot preview scale stable and proportional across robots, and restore Super Mech's visible homing-missile attacks.

## Plan

- [ ] Measure loaded model radii and effective camera distances for representative small, medium, large, boss, and reactor robots.
- [ ] Correct the camera reference/tier calculation so model size maps monotonically to displayed size.
- [ ] Trace Super Mech gun and secondary-weapon scheduling against the base D2 AI firing rules.
- [ ] Fix secondary-weapon selection or projectile lifetime/off-screen handling as required.
- [ ] Extend robot preview introspection and integration checks for proportional scale and Super Mech homing missiles.
- [ ] Run scoped quality checks, Android and Windows builds, CTest, and focused emulator tests.

## Findings

- Pending.

## Validation

- Pending.
