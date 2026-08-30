# Guide-Bot recall to ship

## Goal

Add a Guide-Bot command that routes the deployed companion back to the owning
player's ship, docks it on arrival, and leaves it available for deployment again.

## Plan

- [x] Trace existing Guide-Bot menu, touch command, AI goal, deploy, save, and
  multiplayer behavior
- [x] Define recall and docking state transitions, including cancellation and
  invalid/dead object handling
- [x] Implement the command in the engine and expose it through existing Guide-Bot
  control surfaces
- [x] Add high-level automation/introspection coverage for recall, docking, and
  redeployment
- [x] Run scoped code quality, relevant tests, and Windows CMake builds

## Design notes

- Recall should target the Guide-Bot owner's ship in cooperative games, not an
  arbitrary local player
- Docking should remove the live companion object from play and use the existing
  not-released state so the established deploy path can recreate or reactivate it
- A new objective should not rely on a fixed object index for a moving player ship
- Save/load and ownership behavior must remain coherent while recall is active

## Verification

- Windows D1/D2 CMake build passed
- D2 CTest suite passed, 44/44 tests
- Android debug APK build passed for arm64-v8a, armeabi-v7a, and x86_64
- `test_guidebot_recall_to_ship.jsonc` passed on the emulator, including travel
  from segment 20, docking, and redeployment
- Scoped code quality passed
