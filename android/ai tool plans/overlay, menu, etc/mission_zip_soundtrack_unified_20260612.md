Mission zip soundtrack unified source

Goal:
- Make the overlay and launch policy treat Mission zip as the mission package soundtrack, whether the package provides compressed audio or MIDI/HMP tracks.
- Keep launcher Files and CD as launcher-picked sources only.
- Rename internal availability language away from "builtin music" where practical so the behavior is less misleading.

Plan:
- [x] Create this plan and record the requested behavior.
- [x] Inspect mission zip extraction and soundtrack detection paths.
- [x] Rename/add mission soundtrack availability helpers and update callers.
- [x] Add focused tests for compressed-audio and MIDI mission soundtrack availability.
- [x] Run scoped formatting and focused Android tests.

Notes:
- The user-facing Mission zip source should mean "use mission-provided soundtrack", not "use compressed files only".
- Mission zip soundtrack detection now uses one soundtrack extension set for compressed audio and MIDI/HMP.
