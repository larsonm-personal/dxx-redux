## Touch overlay music menu refactor study

Goal: design a medium-sized refactor for live music source selection, overlay controls,
and game-settings integration without starting implementation yet.

- [x] Trace current overlay music menu UI and controller navigation
- [x] Trace native music source, volume, pause, and track switching paths
- [x] Trace launcher music setting storage and mission zip masking behavior
- [x] Identify live-apply boundaries, risks, and test strategy
- [x] Summarize recommended implementation design and tradeoffs

Notes:
- Current `MusicControlPanel` is track-list only. Source selection and volume need a larger model.
- Launcher storage is split between `music_mode` and
  `use_mission_soundtrack_when_available`; live overlay should preserve that split.
- Existing native sound menu proves live type changes can use `songs_uninit()` followed by
  replaying current level/title music, but that behavior is not exposed as one JNI operation yet.
- Current pause helper uses `do_game_pause()` and pauses music. The new overlay needs a
  gameplay-only pause path, likely a menu/window pause without `songs_pause()`.
