# Robot Preview Firing Roles And Orientation

## Objective

Make attack previews follow robot firing roles and weapon behavior from the loaded game data, including non-firing robots, mine-droppers, per-weapon speed, and model-oriented projectile launches.

## Plan

- [x] Trace D2 gameplay conditions for ordinary firing, mine dropping, companion, thief, and non-firing robots.
- [x] Inspect preview weapon selection and speed integration against the loaded `Robot_info` and `Weapon_info` records.
- [x] Classify preview attack roles from HAM flags, level-object behavior, gun count, weapon identifiers, and the engine's brain slot semantics.
- [x] Suppress fabricated fire for non-firing companions, thieves, brains, and robots without a valid ranged weapon.
- [x] Model mine-droppers by spawning the configured proximity weapon behind the robot with appropriate stationary or low-speed flight behavior.
- [x] Transform muzzle position and projectile direction by the robot's current preview orientation.
- [x] Extend introspection and integration tests for role, weapon speed, and orientation behavior.
- [x] Run scoped quality checks, Android and Windows builds, CTest, and focused emulator coverage.

## Findings

- Normal gameplay only fires when a robot has a valid gun index. The Class 2 Supervisor is also the special `ROBOT_BRAIN` slot, which gameplay explicitly excludes from its firing state.
- D2 companions and thieves are identified by dedicated `robot_info` fields. Their special gameplay may fire flares at doors, but they do not fire their configured robot weapon at the player, so the isolated attack preview classifies them as non-firing.
- Mine-dropping is controlled by `AIB_RUN_FROM` on a level object. D1 and D2 both reverse the robot forward vector and create a proximity bomb; D2 can select `ROBOT_SUPERPROX_ID` through `SUB_FLAGS_SPROX`.
- Original base HAM robot records do not retain the level-object `RUN_FROM` assignment. Base robot 10 therefore needs a documented canonical fallback, while level-backed mod previews use the loaded behavior and D2 super-proximity subflag.
- The Seeker uses weapon 55 at speed 70 on difficulty 1, while the Medium Hulk uses weapon 22 at speed 50. The old display divided travel by circle distance; the Seeker's circle distance of 57 almost exactly canceled its authored speed advantage relative to the Hulk's minimum display distance of 40.
- Projectile simulation now retains loaded speed, speed variance, thrust, drag, and homing behavior but uses a fixed 40-unit display distance, independent of robot circle distance.
- Ranged shots start at the animated gun point calculated by `calc_gun_point`. Ranged and mine launch vectors use the robot's current heading and pitch; mines reverse that vector.

## Validation

- Scoped formatting and PowerShell analysis passed.
- Android `assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64.
- Focused `RobotPreviewRequestStoreTest` passed.
- Windows D1 and D2 builds completed successfully.
- CTest passed 33 of 33 D1 tests and 40 of 40 D2 tests.
- D2 base robot 10 passed as a mine-dropper using proximity weapon 16, with an orientation-dependent drop vector.
- D2 base robots 7, 33, and 42 passed as non-firing with zero fabricated shots.
- D2 base robot 43 passed as ranged using weapon 55; its initial preview speed was 70, two orientation-dependent shots rendered through the actual weapon model, and no fallback rendered.
- D1 base robot 0 passed ranged weapon rendering and orientation checks.
- The D2 mod-backed preview passed; its selected Guide Bot replacement was classified non-firing from loaded companion data.
