Touch overlay music refresh and controls

Goal:
- Make the overlay music track list update after live source changes.
- Replace the tiny close control with a tappable Close button on the source row.
- Improve the volume slider's visible affordance.

Plan:
- [x] Read local instructions and inspect current overlay/source-switch paths.
- [x] Add delayed source-switch refreshes so the track list follows the applied source.
- [x] Move Close onto the source row with a larger hit target.
- [x] Add a visible volume slider outline and larger position indicator.
- [x] Run scoped formatting and focused Android verification.

Notes:
- Source switching is intentionally queued to the game thread, so the panel needs a small refresh delay rather than immediate full native refresh.
