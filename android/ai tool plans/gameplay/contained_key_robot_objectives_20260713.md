# Contained-Key Robot Objectives

## Goal

Represent a key carried by a robot as a distinct player objective so metadata, Guide-Bot guidance, and the automap explain that the carrier must be destroyed, while preserving the existing shared route and classic Guide-Bot movement.

## Plan

- [ ] Trace contained-key identity through shared topology targets, semantic route steps, metadata JSON, live Guide-Bot guidance, and automap objective rendering.
- [ ] Preserve the carrier object's stable identity on contained-key route steps without changing ordinary loose-key behavior.
- [ ] Give contained-key steps concise destruction guidance that names the key color and carrier robot.
- [ ] Make automap objective markers resolve the current carrier position every frame so moving robots remain moving objectives.
- [ ] Cover D1 and D2 contained-key semantics in native tests and KCXF2 level 6 in a focused integration fixture.
- [ ] Run scoped quality, Windows D1/D2 builds and host tests, metadata corpus gates, Android JVM/all-ABI build, and the focused emulator test.
- [ ] Update the master unification plan and this plan with final results.

## Boundary

The Guide-Bot only navigates toward the high-level objective and tells the player what to do. It must not aim at, fire on, chase through a new movement path, or damage the carrier robot. Object tracking is presentation and high-level route-state data only.
