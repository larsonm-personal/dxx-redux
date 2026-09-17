# Secret-level robot HUD

- [x] Trace D1/D2 HUD suppression and secret-level counter initialization
- [x] Show robot counts in secret levels while preserving existing hostage display behavior
- [x] Initialize D2 first-visit secret robot counters; keep saved counters on return visits
- [x] Extend shared renderer regression coverage and run scoped quality, both host builds, and HUD tests

The D2 HUD explicitly selects secrets-only rendering for negative level numbers
Co-op initializes destination counters through the normal level path, but the single-player
StartNewLevelSecret path does not initialize the robot counters on a first visit
Secret save/restore already preserves the destination's num_kills_level and num_robots_level

Validation: scoped run-code-quality.ps1 -Fix passed; run-windows-build.ps1 -Target both
passed without new compiler warnings; test_hud_counts and test_hud_layout passed for
both games (four tests total); git diff --check passed
Android device reproduction and secret transition playthrough were not run
Existing single-player secret saves retain their stored counters
