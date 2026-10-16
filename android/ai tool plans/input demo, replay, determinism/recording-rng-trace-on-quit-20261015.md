# Preserve recording RNG traces on game exit

- Evidence: the supplied D2 level 7 recording has 2,161 frames and advances the
  simulation RNG counter from 0 to 8,066, but its sidecar reports zero events
- Both games unload replay state on window close before flushing the recorder
- Replay reset unconditionally stops the shared RNG trace, clearing recording
  events even when no replay session is loaded

Completed:

1. Add a recorder/replay lifecycle regression covering per-frame state off and on,
   saved RNG events, repeated unload, and actual replay trace finalization
2. Reproduce the failure, then make replay cleanup stop only its own trace
3. Format the changed code, build both game targets, and run relevant native tests

Validation:

- The new lifecycle regression failed before the fix with
  `replay unload discarded the recording RNG trace`
- Scoped code quality checks passed
- D1 and D2 Windows desktop and headless targets built without warnings
- Recorder, replay, and RNG trace tests passed for both games (six tests total)
- The lifecycle regression verifies saved RNG events with per-frame state off
  and on, repeated unload, and finalization of an actual replay trace
- No Android APK was built or installed

The supplied files remain unchanged; their original RNG call-site events cannot
be recovered from the empty sidecar
