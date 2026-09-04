# Counterstrike level 12 post-gold door stall

## Goal

Identify and fix the GuideBot simulation stall at the ordinary-looking door
after the gold key in Counterstrike level 12.

## Plan

1. Inspect the level 12 route metadata and checked-in simulation result.
2. Reproduce the stall with the current Windows route engine and capture the
   post-gold-key route frontier, wall flags, flare selection, and bump recovery.
3. Determine whether the door forbids GuideBot activation or whether flare
   discovery/activation is incorrectly excluding it.
4. Implement the smallest shared correction and add a focused deterministic
   route regression.
5. Rebuild D2, run focused and neighboring route tests, run native tests and
   scoped quality checks, and update only level 12 simulation output.

## Status

- [x] Metadata inspected
- [x] Stall reproduced and diagnosed
- [x] Fix and regression implemented
- [x] Build and tests passed
- [x] Simulation record updated

## Result

The apparent ordinary door is an asymmetric auto-closing portal. Its entry
face is unlocked, but its return face is locked until trigger 7 fires. The old
plan collected the gold key first, allowing the door to close behind the
GuideBot before asking it to fire trigger 7.

Static-key planning now checks every traversed edge for a switch-controlled,
non-passable return edge after prerequisite resolution. It activates that
switch before crossing. Transiently open locked doors are also excluded from
both metadata and live-certifier passability.

Counterstrike level 12 now plans switch 7 before the gold key and completed two
identical engine runs in 11,595 frames. The D2 Windows build, all 45 native
tests, scoped code quality checks, and focused level 6, 11, and 12 simulations
passed.
