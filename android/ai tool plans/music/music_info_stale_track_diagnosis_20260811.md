# Music info stale selected track diagnosis

## Goal

Determine why the in-game music info menu reports the previously playing mission ZIP MIDI after choosing a new track in the track selector.

## Plan

- [ ] Trace the track selector, playback transition, and music info data sources in D1 and D2
- [ ] Identify the repeatable stale-state sequence and confirm the responsible state update or ordering
- [ ] Report the root cause with supporting file and line references; note the smallest safe fix and test coverage

## Scope

Diagnosis only. Do not change game behavior unless the user subsequently requests a fix.
