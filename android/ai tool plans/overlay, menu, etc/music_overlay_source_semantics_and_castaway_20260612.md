Touch overlay music source semantics and Castaway debugging

Goal:
- Make the close button touchable by giving the volume slider a dedicated right-side lane.
- Add scroll indicator icons to the track list.
- Debug Castaway mission selection hanging/refusing selection.
- Correct music source semantics so mission zip music is distinct from launcher-picked files and launcher-picked CD audio.
- Hide unavailable launcher-picked files/CD choices, and rename MIDI to Base game MIDI.

Plan:
- [x] Create this plan and record the requested behavior.
- [x] Inspect current overlay layout, source availability, and launcher music config paths.
- [x] Trace Castaway mission metadata and mission-music masking behavior.
- [x] Implement UI availability/layout/label fixes.
- [x] Fix source semantics so mission music is not exposed as launcher-picked files.
- [x] Run scoped formatting plus focused Kotlin/native verification.

Notes:
- The intended categories are mission zip, launcher-picked files, launcher-picked CD, and base game MIDI.
- If launcher-picked files or CD audio are not configured, those options should not be shown.
- Castaway source switching exposed a native lifetime bug: songs_uninit freed BIMSongs without clearing it, so replaying built-in mission music could double-free during songs_init.
