# Guidebot Locked Wheel Spawn

Goal: let D2 touch users spawn or release the Guide-Bot from the locked Guide wheel by dragging to an outer ring action.

- [x] Inspect existing Guide wheel locked state and native escort ownership helpers
- [x] Add a game-thread native helper to spawn or release the Guide-Bot at the player
- [x] Add a locked Guide wheel outer-ring action with matching border, highlight, and activation behavior
- [x] Add focused tests or validation for the touch selection behavior
- [x] Run scoped formatting and practical build/test checks

Follow-up fix:
- [x] Consume the pending Guide-Bot spawn/release action on gameplay idle frames rather than only from `HandleGameKey()`, so touch-only activation fires without needing a simultaneous keyboard event.
