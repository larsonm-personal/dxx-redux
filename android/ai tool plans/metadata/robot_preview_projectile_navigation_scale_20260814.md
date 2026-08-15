# Robot Preview Projectile, Navigation, and Scale Refinements

## Objective

Refine robot preview projectile cleanup, mine retention, mod navigation, and camera scale while preserving loaded game and mod data as the source of truth.

## Plan

- [x] Trace preview-space projectile motion and establish a consistent off-screen retirement boundary for every render type.
- [x] Limit mine-dropping robots to their two newest active mines without affecting other projectiles.
- [x] Carry the changed-robot number list from metadata replacement rows into the native preview request.
- [x] Make Next and Previous wrap through the supplied changed-robot list for mods while retaining the full catalog for base games.
- [x] Replace per-robot radius normalization with stable normal and large-object camera tiers.
- [x] Extend introspection and the robot preview integration test for projectile retirement, mine caps, navigation lists, and camera tiers.
- [x] Run scoped quality checks, Android and Windows builds, CTest, and focused emulator tests.

## Findings

- The preview previously retired a projectile only at one fixed right-side target or when its weapon lifetime expired. Head-on blob and particle weapons therefore traveled toward and through the camera until timeout.
- The model-picture callback provides the exact rendered robot position. Comparing each projectile against that camera-space position gives one retirement rule for bitmap, vclip, model, and fallback-particle weapons.
- Preview mines used the generic 32-projectile pool with no retention policy. Assigning every spawn a sequence and retiring the oldest active mine before a third drop keeps exactly the newest two.
- Mod preview requests previously contained only the selected robot. The metadata row already contains the complete changed-robot set, so the request now carries its distinct sorted robot numbers to native navigation.
- The stock model-picture helper scales camera distance directly with every model radius, making all robots appear approximately the same size. A fixed reference radius preserves relative size.
- Robot 16 supplies the normal reference size. Bosses, D2 robot 65, and any model larger than the normal reference use a shared large-object reference based on the largest loaded large model.

## Validation

- Scoped `run-code-quality.ps1 -Fix` passed for the shared C++, Kotlin, PowerShell, and plan files.
- `git diff --check` passed.
- Android debug APK build passed for arm64-v8a, armeabi-v7a, and x86_64.
- Windows D1 and D2 builds passed with `run-windows-build.ps1 -Target both`.
- CTest passed 33 of 33 D1 tests and 40 of 40 D2 tests.
- D2 emulator tests passed for Class 1 Driller off-screen retirement, Class 3 Gopher two-mine retention, Obsidian modified-robot navigation, and Alien 1 Boss large-camera/off-screen behavior.
- D1 base-game robot 0 emulator test passed off-screen retirement, orientation-dependent firing, navigation, animation, sound, and preview shutdown checks.
