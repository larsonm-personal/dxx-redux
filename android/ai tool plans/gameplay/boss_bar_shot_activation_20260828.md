# Boss Bar Shot Activation

Date: 2026-08-28

## Goal

Show the boss health bar only after the boss and player have directly exchanged weapon fire. A shot must activate the bar even when invulnerability or weapon immunity prevents damage. In cooperative games, propagate activation to every player when the existing multiplayer protocol supports a small, safe change.

## Phase 1: Trace Existing Behavior

- [x] Find every boss bar activation, reset, rendering, weapon collision, and multiplayer synchronization path in D1 and D2
- [x] Identify the causes of activation before the player has interacted with the boss
- [x] Select the smallest shared activation contract that covers player-to-boss and boss-to-player shots before damage filtering

## Phase 2: Implement Shot-Based Activation

- [x] Remove or bypass non-shot boss bar activation triggers
- [x] Activate on a player weapon colliding with the boss, including zero-damage and immune hits
- [x] Activate on a boss weapon colliding with the player, including player invulnerability
- [x] Keep D1 and D2 behavior aligned while matching each engine's existing style

## Phase 3: Cooperative Propagation

- [x] Reuse or minimally extend multiplayer messaging so one player's activation turns on the bar for all cooperative players
- [x] Validate packet inputs and avoid changing competitive-mode behavior

## Phase 4: Regression Coverage and Verification

- [x] Add focused automated coverage for activation and non-activation behavior where practical
- [x] Run scoped code quality checks on changed files
- [x] Run relevant tests and successful CMake builds for D1 and D2
- [x] Record completed work and verification results in this plan

## Completed Behavior

- The HUD no longer scans for damaged bosses and no longer activates from AI firing, melee contact, teleporting, replicated firing, or direct damage helpers
- Accepted player-weapon collisions activate before D2 boss weapon-immunity filtering reaches damage application
- Accepted boss-weapon collisions activate before player invulnerability reaches damage application
- Cooperative activation uses boss action code 6 and is sent only for the local shooter's boss hit or the local player's boss-weapon hit
- Competitive modes do not send the HUD action
- Multiplayer protocol revisions are D1 30017 and D2 30018 so mixed builds cannot interpret the new action as invalid

## Verification

- Scoped `android/run-code-quality.ps1 -Fix` passed for all changed C, C++, Kotlin, JSONC, and paired engine paths
- `run-windows-build.ps1 -Target both` completed for D1 and D2
- D2 CTest passed all 44 tests; the D1 build currently registers no CTest tests
- `gradlew.bat :app:assembleDebug` completed successfully for the configured Android ABIs
- `test_boss_health_bar.jsonc` passed 38 of 38 steps in D1 on the emulator
- `test_boss_health_bar.jsonc` passed 38 of 38 steps in D2 on the emulator, with the level-start invulnerability active during the boss-to-player shot case
