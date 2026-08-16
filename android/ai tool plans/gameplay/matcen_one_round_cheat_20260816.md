# Matcen one-round cheat

Date: 2026-08-16
Status: complete

## Request

- Add an automap checkbox named `matcens: 1 round limit`
- Highlight the option green when enabled without changing its text
- Limit every matcen to one total activation while enabled without setting the
  engine cheater flag
- Preserve ordinary activation accounting so disabling the option restores each
  matcen's remaining normal limit
- Save and restore the option in save games, including the automap display state
- Replace the checkbox boolean with three states: default, one-round limit, and
  paused
- When entering or leaving paused mode, force every matcen inactive while
  preserving its remaining lives
- Synchronize the mode in multiplayer

## Plan

- [x] Trace the paired D1/D2 automap controls, matcen activation accounting, and
  save-game extension points
- [x] Implement the runtime toggle and one-round activation cap in both engines
- [x] Persist and restore the setting in D1/D2 save games
- [x] Add focused regression coverage for toggle behavior and save/load state
- [x] Run scoped formatting, focused tests, and paired D1/D2 build validation

## Tri-state and multiplayer extension

- [x] Trace host authority, reliable multiplayer control messages, join/restore
  synchronization, and the matcen active-cycle reset requirements
- [x] Replace the boolean runtime/save/UI state with the three-state cycle and
  implement pause transition semantics
- [x] Add authoritative D1/D2 multiplayer propagation and late-join state sync
- [x] Extend focused policy, save metadata, and UI regressions
- [x] Run scoped formatting, focused tests, Android assembly, and paired D1/D2
  build validation

## Constraints

- Do not set the cheater game-engine flag
- Preserve unrelated working-tree changes
- Keep the D1 and D2 implementations behaviorally aligned

## Results

- The single-player automap settings tray now includes the fixed-label checkbox
  `matcens: 1 round limit`; its checked state uses the existing green latched
  background and green check mark
- Every successful matcen activation is tracked from level start, including
  activations before the option is enabled and D2 Insane activations
- While enabled, each matcen is blocked after its first activation; disabling
  the option restores the engine's ordinary remaining `Lives` behavior
- New games clear the option, level transitions preserve the option while
  clearing per-level activation history, and save restores recover both the
  option and activation counts from Android save metadata version 5
- The implementation never changes `cheats.enabled` or any player cheater flag
- The automap option now cycles through the exact labels `matcens: default`,
  `matcens: 1 round limit`, and `matcens: paused`; both non-default states use
  the existing green checked presentation
- Entering or leaving paused mode disables all active matcens through the
  engine's existing shutdown path without changing their remaining lives
- Multiplayer clients request changes from the host, which validates the
  sender and broadcasts the authoritative mode plus activation history; the
  same state is sent directly to late joiners
- Scoped code quality, D1 and D2 Windows builds, paired tri-state mode and save
  metadata tests, the admin-tray UI test, and debug APK assembly passed
