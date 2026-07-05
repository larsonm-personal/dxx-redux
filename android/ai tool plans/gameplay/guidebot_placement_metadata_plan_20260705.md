# Guidebot Placement Metadata Plan

## Goal
Add explicit D2-level metadata that records whether a level has a guidebot/start cage placed in it, separate from whether the guidebot can currently be reached from the player start.

## Steps
- [x] Review current guidebot metadata plumbing and repo instructions.
- [x] Add scanner fields for guidebot placement and placement notes.
- [x] Serialize placement metadata through JNI, headless dumps, launcher models, and generated mission JSON.
- [x] Show the placement note in the level metadata detail view.
- [x] Extend metadata scan tests for present, missing, and unreachable guidebot cases.
- [x] Regenerate focused mission metadata JSON outputs used in this investigation.
- [x] Run scoped formatting, native tests, and launcher build checks.

## Notes
- Keep the existing reachability note for the guidebot as a separate concern.
- Limit this to D2-style scanner views by keying off the guidebot companion callback.
