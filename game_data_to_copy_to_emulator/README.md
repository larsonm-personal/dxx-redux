# Game Data for Emulator Testing

Place files here for `push_game_data.sh` to push to the Android emulator.

## Subfolders

- **data/** -- Game data files (HOG, PIG, HAM, MVL, etc.) and music files
  (BIN/CUE, GOG/INST). Pushed to the app's active file set (`sets/default/`).
- **download/** -- Files that would normally be downloaded or imported via the
  file picker (e.g. GOG installers, disc images). Pushed to `/sdcard/Download/`
  so they're visible in the Android file picker.

Both subdirectories are gitignored; place your files directly inside them.
