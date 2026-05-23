# Game Menu Touch and Weapon Wheel Labels Plan

## Goals
- Dismiss the native ESC/game menu with a single tap outside the menu area on Android
- Show the current laser level in the primary weapon wheel as `Laser\nlvl N`
- Make D2 weapon wheel slot labels reflect the weapon the slot will actually select, not just the base-tier slot name

## Plan
1. [completed] Trace the Android native game-menu touch handling path and the touch-wheel label path
2. [completed] Patch Android native newmenu handling so outside taps dismiss only the ESC/game menu
3. [completed] Patch touch-wheel labels to use native weapon state, including laser level and D2 paired-weapon selection rules
4. [completed] Re-run focused Android compile and Kotlin unit validation
