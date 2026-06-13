Touch overlay MIDI to mission freeze

Goal:
- Fix the freeze when launching with MIDI, then switching the overlay source to mission music.
- Ensure switching to mission has an actual effect while the overlay remains open.
- Preserve diagnostics around source switching.

Plan:
- [x] Record the testing issue and create this plan.
- [x] Inspect the overlay refresh and native source-apply path for UI-thread blocking or missed game-thread consumption.
- [x] Implement the smallest source-switch fix that avoids freezing and updates the live music.
- [x] Run scoped formatting and focused Android verification.

Notes:
- Reported behavior: MIDI launch, switch to mission, UI freezes and music does not change. Phone Back unfreezes, re-selects MIDI, and restores behavior.
