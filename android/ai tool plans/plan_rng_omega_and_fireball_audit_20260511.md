# RNG omega and fireball audit 2026-05-11

## Goal
- continue the single-player `_fx()` audit from the current survey state
- resolve whether omega blob lifetime can ever be effect-only
- confirm whether nearby fireball/object helper sites stay `_fx()` or need reclassification

## Steps
- [completed] inspect omega blob lifetime in d2 `laser.c` and trace that the affected `OBJ_WEAPON` can deal damage and alter collision timing
- [completed] record the omega result in the main survey notes
- [completed] inspect the next nearby single-player object-creation RNG site by tracing debris creation and collision handling in `fireball.c` and `collide.c`
- [completed] inspect the robot contained-drop RNG in `fireball.c` and confirm it stays on the sim stream
- [completed] inspect the shared `drop_powerup()` RNG in `fireball.c` and confirm it stays on the sim stream
- [completed] inspect the D2 `spit_powerup()` weapon-drop RNG and confirm it stays on the sim stream
- [completed] inspect explodable-wall fireball position RNG in `fireball.c` and confirm it stays on the sim stream
- [completed] run focused validation on the omega and debris note updates

## Notes
- user guidance: if omega lifetime has anything to do with dealing damage or hitting robots, it should not be treated as ambiguous
- confirmed result: omega blob lifetime stays on SIM because `create_omega_blobs()` assigns the random lifetime to real `OBJ_WEAPON` `OMEGA_ID` objects, the last blob is explicitly positioned to cause damage, and `collide.c` special-cases omega damage on collision paths
- confirmed result: debris creation RNG stays on SIM because `create_debris_object()` assigns randomized velocity, rotation, and lifetime to real `OBJ_DEBRIS` physics objects, and `collide_weapon_and_debris` lets player weapons hit and explode them in both D1 and D2
- confirmed result: robot contained-drop RNG stays on SIM because `fireball.c` uses it to decide whether a destroyed robot drops contained items and how many pickups `object_create_egg()` spawns in both D1 and D2
- confirmed result: `drop_powerup()` RNG stays on SIM because it randomizes the velocity and short-lived pickup lifetime of real dropped `OBJ_POWERUP` objects in both D1 and D2
- confirmed result: D2 `spit_powerup()` RNG stays on SIM because weapon-drop code spends sim RNG for the seed and then uses that seed to randomize the velocity and lifetime of real dropped `OBJ_POWERUP` objects
- confirmed result: explodable-wall fireball position RNG stays on SIM because the random wall-face positions feed both ordinary explosion spawns and periodic `object_create_badass_explosion()` calls with nonzero damage and force in both D1 and D2
