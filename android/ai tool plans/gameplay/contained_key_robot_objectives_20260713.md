# Contained-Key Robot Objectives

## Goal

Represent a key carried by a robot as a distinct player objective so metadata, Guide-Bot guidance, and the automap explain that the carrier must be destroyed, while preserving the existing shared route and classic Guide-Bot movement.

## Plan

- [x] Trace contained-key identity through shared topology targets, semantic route steps, metadata JSON, live Guide-Bot guidance, and automap objective rendering.
- [x] Preserve the carrier object's stable identity on contained-key route steps without changing ordinary loose-key behavior.
- [x] Give contained-key steps concise destruction guidance that names the key color and carrier robot.
- [x] Make automap objective markers resolve the current carrier position every frame so moving robots remain moving objectives.
- [x] Cover D1 and D2 contained-key semantics in native tests and KCXF2 level 6 in a focused integration fixture.
- [x] Run scoped quality, Windows D1/D2 builds and host tests, metadata corpus gates, Android JVM/all-ABI build, and the focused emulator test.
- [x] Update the master unification plan and this plan with final results.

## Boundary

The Guide-Bot only navigates toward the high-level objective and tells the player what to do. It must not aim at, fire on, chase through a new movement path, or damage the carrier robot. Object tracking is presentation and high-level route-state data only.

## Results

- Shared route steps now retain a robot carrier object number and use `destroy_key_carrier` with labels such as `Destroy robot carrying blue key`. Ordinary loose keys remain `pickup_key` objectives, and key completion still requires the player to own the key.
- Numbered automap objectives resolve the carrier's live object position on every draw. After the robot is destroyed, the marker follows the nearest matching loose key so the objective remains useful during the pickup transition.
- Guide-Bot consumes the same shared semantic step and reports the destruction instruction. No aiming, firing, collision, steering, classic path construction, or RNG behavior was added or changed for this objective.
- The regenerated corpus contains 145 contained-key objectives across 99 levels, including Descent level 20, Counterstrike levels 10, 17, and 21, and KCXF2 level 6. Strict regeneration passed Counterstrike plus 109 processable archives with one expected descriptor-less skip and zero failures; route status, route problems, ordering, and distances were unchanged.
- Supported Windows builds passed 19 D1 and 22 D2 CTests. Android JVM tests, all three native ABIs, the 1,274-level fingerprint corpus, and the automation catalog passed. The 41-step KCXF2 level 6 emulator fixture verified object 81, the moving automap marker, prerequisite progression, and the exact Guide-Bot instruction and console message.
