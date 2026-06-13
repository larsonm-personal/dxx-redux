Music one-track checkbox snap behavior

Goal:
- Make the overlay "One track per level" checkbox selectable by touch and controller.
- When changed from off to on, snap playback to the correct track for the current level.
- If already on and the user manually changed tracks, do not auto-snap unless the option is turned off and back on.

Plan:
- [x] Create this plan and record the requested behavior.
- [x] Inspect overlay hit handling and native play-order handling.
- [x] Add explicit touch capture for the checkbox row.
- [x] Add native off-to-on snap behavior and preserve no-op behavior while already selected.
- [x] Run scoped formatting and focused Android/native checks.

Notes:
- The existing native setter replays when changing play order, but it does not distinguish off-to-on snapping from other changes explicitly.
