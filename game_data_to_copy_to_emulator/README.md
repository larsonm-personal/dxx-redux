# Game Data for Emulator Testing

Place files here for `android/helpers/push_game_data.sh` to push to the Android emulator.

## Subfolders

- **data/** -- Game data files (HOG, PIG, HAM, MVL, etc.) and music files
  (BIN/CUE, GOG/INST). Pushed to the app's active file set (`sets/default/`).
- **download/** -- Files that would normally be downloaded or imported via the
  file picker (e.g. GOG installers, disc images). Pushed to the dedicated
  `/sdcard/Download/dxx-redux-test-data/` folder so they remain visible in the
  Android file picker without sharing a cleanup boundary with personal downloads.

Both subdirectories are gitignored; place your files directly inside them.

The default command is additive. Missing or empty local folders never remove
device files. To remove files previously created by this helper but no longer
present locally, set `ANDROID_SERIAL` explicitly and run
`bash android/helpers/push_game_data.sh --sync-owned`. That mode consults the
helper's durable ownership manifests and never deletes unowned app or Download
contents. Clear the app/emulator explicitly when a completely clean state is
required.
