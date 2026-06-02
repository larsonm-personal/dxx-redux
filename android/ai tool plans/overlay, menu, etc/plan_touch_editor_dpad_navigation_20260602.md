# Touch Editor D-pad Navigation Plan - 2026-06-02

## Goal
Make the touch overlay settings editor usable from a controller-only device:
- Default D-pad focus should start on Close Editor in the bottom tray.
- Left and right should navigate across bottom tray actions with green focus highlighting.
- Copy to new slot should let up and down escape the slot-name text field.
- Global settings opacity slider should use left and right for value changes, while up and down move to nearby actions.

## Steps
1. [done] Map existing touch editor focus and dialog behavior.
2. [done] Add explicit focus requesters and key routing for the bottom tray and dialogs.
3. [done] Add or update focused unit tests for navigation helpers where practical.
4. [done] Run Android unit tests or the narrowest available Gradle check.

## Notes
- Bottom tray focus now starts on Close Editor and left or right moves between tray actions, skipping disabled Save.
- Preset picker seeds focus on the first preset so controller-only devices can load presets.
- Copy to new slot and New Slot text fields can move down to OK when a name exists, or Cancel while blank, and up to Cancel.
- Global Settings keeps left and right for opacity changes while vertical D-pad no longer changes the slider. Down moves to OK.
