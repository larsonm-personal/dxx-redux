# Cooperative duplicate weapon pickup

## Goal

Allow a cooperative player to collect a weapon they already own for the normal
duplicate-weapon energy bonus when every connected player owns that weapon.

## Plan

- [x] Trace duplicate weapon pickup handling and cooperative player ownership in
      both D1 and D2
- [x] Implement the smallest matching D1 and D2 gameplay changes
- [x] Evaluate focused regression coverage
      - The behavior depends on synchronized two-peer engine state and has no
        isolated unit seam; validation used both engine builds and the Android
        native builds
- [x] Run scoped formatting, builds, and relevant tests
- [x] Record completed work and verification results here

## Completed work

- Duplicate Spreadfire, Plasma, and Fusion pickups in D1 now become the normal
  energy bonus when every non-disconnected cooperative player owns the weapon
- D2 applies the same rule to Spreadfire, Plasma, Fusion, Helix, Phoenix, and
  Omega
- Competitive multiplayer still leaves duplicate weapons for other players
- Cooperative inventory status is broadcast to every peer on all platforms,
  and successful primary pickups schedule an immediate status update
- D2 ship status now carries the high primary-weapon flag bits needed for
  Phoenix and Omega

## Verification

- Scoped code-quality checks passed
- Android `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- Android `:app:testDebugUnitTest` passed
- Windows D1 and D2 game binaries compiled successfully
- The Windows all-target build remains blocked by the pre-existing
  `test_coop_player_session` target missing SDL/PhysFS include directories;
  this occurs after the game targets build
