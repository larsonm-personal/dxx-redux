Touch overlay music source dropdown

Goal:
- Replace the overlay music menu's four separate source buttons with a single drop-down source picker.
- Keep all source changes live while the game is running, including mission soundtrack masking behavior and persisted preferences.
- Preserve controller navigation for play/pause, one-track-per-level, source selection, volume, and the scrollable track list.

Plan:
- [x] Read local instructions and inspect the current music overlay panel.
- [x] Convert source selection in the overlay to one drop-down control.
- [x] Support touch and controller selection for the expanded source list.
- [x] Run scoped formatting and focused Android verification.

Notes:
- This is the next UI slice of the original design. The native source switching and player-file persistence backing were implemented in the previous slices.
