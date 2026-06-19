# Death Saying Keyboard Back Fix

## Problem
- [ ] On the game-over "Enter your cool saying" input menu, closing the Android soft keyboard can leave the player stuck because tapping the input again does not reopen it.
- [ ] Android Back should still act like Escape for this menu path, closing the saying prompt when the keyboard is not consuming Back.

## Plan
- [x] Read project instructions and locate the Android keyboard/newmenu paths.
- [x] Inspect input menu selection, mouse/touch handling, and soft keyboard state tracking in D1 and D2.
- [x] Patch the shared menu input behavior so tapping the active text field can request the keyboard again.
- [x] Verify Android Back still maps to Escape and that the score/saying menu closes through the existing path.
- [x] Run focused formatting/build or explain any verification limits.

## Notes
- Keep D1 and D2 menu behavior in sync unless the code paths differ.
- Prefer Android-specific hooks in the existing `newmenu.c` and `android_input.c` paths.
