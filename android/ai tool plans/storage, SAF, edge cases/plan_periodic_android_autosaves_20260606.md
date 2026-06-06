# Periodic Android Autosaves Plan

## Goal

Add two timed Android single-player autosave slots that behave like a 5-minute ring buffer:

- newest timed autosave is 0-5 minutes old
- previous timed autosave is 5-10 minutes old
- when the newest save ages past 5 minutes, write the other slot and rotate

## Slot Choice

Existing Android special slots:

- slot 6: auto abort
- slot 7: best progress
- slot 8: auto exit
- slot 9: auto minimize

Use slots 4 and 5 for timed autosaves. These are the lowest available numbers at the end of the 10-slot save list without colliding with the existing special autosaves.

## Implementation

1. [x] Add metadata kind `ANDROID_SAVE_META_KIND_AUTO_PERIODIC`.
2. [x] Add slot constants:
   - `ANDROID_SAVE_META_SLOT_AUTO_PERIODIC_A = 4`
   - `ANDROID_SAVE_META_SLOT_AUTO_PERIODIC_B = 5`
3. [x] Add shared helper in `state_android_shared.c`:
   - track next periodic save time and next slot
   - reset when mission, level, or callsign changes
   - skip multiplayer, non-level gameplay, dead player, D2 secret levels, and D2 final boss death sequence
   - call `state_android_save_to_slot()` with desc `AUTO 5MIN`
4. [x] Add tiny Android-only calls in D1/D2 `game.c` once per gameplay frame.
5. [x] Validate with host/Android builds and formatting.

## Progress

- [x] Add constants and helper
- [x] Wire D1/D2 gameplay loops
- [x] Validate

## Validation

- `.\android\tests\test_native_host_unit_tests.ps1`
- `.\android\run-code-quality.ps1 -Fix`
- `.\gradlew.bat :app:externalNativeBuildDebug`
