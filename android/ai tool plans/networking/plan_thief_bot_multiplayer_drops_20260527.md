# Multiplayer Thief Bot Drop Audit Plan

## Goal
Check whether the D2 thief bot drops all stolen multiplayer items, including items stolen from multiple players, and make multiplayer QoL or full death spew force eligible thief drops to 100 percent when enabled

## Steps
- [ ] Trace thief bot steal and drop state in D2, including multiplayer owner handling
- [ ] Fix stolen item drop ownership or chance behavior at the source if needed
- [ ] Add focused coverage where practical, or document why a build-only validation is the right scope
- [ ] Run formatting and targeted build or test validation

## Notes
- Do not edit android/outstanding_bugs.md
- Keep source of truth in game code, not Kotlin
- D1 has no thief bot, so expect this to be D2-focused unless shared hooks are involved