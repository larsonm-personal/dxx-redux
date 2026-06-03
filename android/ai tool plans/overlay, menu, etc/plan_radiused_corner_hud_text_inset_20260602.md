# Radiused Corner HUD Text Inset Plan

## Goal
- Make rounded-corner HUD text movement depend on each text/icon rectangle's vertical position.
- Preserve tri-state behavior: off, half inset, full inset.
- Add afterburner text, key icons, and in-game clock text to the same potential-offset logic.

## Steps
- [x] Pass per-corner rounded-corner measurements from Kotlin to native.
- [x] Replace flat native left/right inset helpers with y-aware radiused helpers.
- [x] Update D1/D2 HUD calls to pass y/height and include afterburner, keys, and clock.
- [x] Run formatting and targeted verification.

## Notes
- Keep y coordinates unchanged. Only x positions should move.
