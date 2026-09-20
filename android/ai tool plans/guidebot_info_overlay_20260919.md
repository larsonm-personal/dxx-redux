# Guidebot info overlay

- [x] Add passive Android diagnostics: current routing, progress, errors, three recent transitions
- [x] Add Guidebot Info checkbox to touch Settings tray for D2
- [x] Run scoped quality checks and relevant native/Kotlin builds

Keep history bounded and coalesce repeated events. Green means an active high-level objective route; red means base objective routing or an error. Idle, return-to-player, and remote ownership are neutral. Collect on the game thread even while hidden and reset on level/save lifecycle changes. No routing behavior changes.

Validation:
- Scoped code quality checks passed
- Android debug APK built for all configured ABIs; 1,056 JVM tests passed
- Windows D1 and D2 builds passed (existing weapon.c return-path warnings)
- test_guidebot_info_overlay.jsonc passed all 34 steps on emulator-5554
- Visually checked green high-level routing and red base routing; final panel sits above touch controls

The overlay reports active high-level objectives versus base objective navigation. Base commands also use red; follow/idle, inactive, and remote-owner states are neutral. Possible stalls require three seconds without movement during objective navigation. The three newest events remain available while hidden, with consecutive identical events counted together. Routing behavior is unchanged.
