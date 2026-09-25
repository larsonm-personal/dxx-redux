# Launcher media controls

Implement Android media-button ownership for launcher music previews without changing the game engines

- [x] Add a shared foreground preview media session and audio-focus lifecycle
- [x] Connect MIDI/HMP/FM, compressed audio, and CD players with explicit transport commands
- [x] Navigate displayed track lists; single-track next/previous seeks +/-10 seconds while retaining pause state
- [x] Stop and release ownership on dismissal, completion, launcher departure, or playback failure
- [x] Exercise transport and lifecycle integration, format changed files, and build Android

Multi-track skips start the adjacent track, do not wrap, and consume commands at boundaries
Manual pause retains media controls so hardware Play can resume
Background playback and game media controls are outside this change
Preserve existing uncommitted music-editor source changes and do not edit outstanding_bugs.md


Implementation notes:
- Framework MediaSession avoids a new media dependency and leaves the native players intact
- Activity media-key dispatch handles foreground keys; MediaSession handles system/headset commands
- Keyed Compose previews reset player state when the selected track changes
- Archive staging jobs are canceled by pause/stop/focus loss/dismissal
- Renderer start reservations prevent a dismissed MIDI load from starting later

Validation:
- Scoped code quality: passed
- MIDI/CD synchronization and audio lifecycle Python suite: 33/34 passed
- The remaining lifecycle test expects `ModManager(filesDir, context)` in SetupSections.kt;
  HEAD already uses `ModManager(filesDir, context, setDir)`, so this is an existing stale assertion
- Android x86_64 debug APK and CMake build: passed, final build 59 seconds
- Kotlin unit suite: 1086 tests, 0 failures/errors, 1 skipped
- Android API 34 emulator: system-dispatched media-key integration passed for MIDI, CD and MP3
  (final APK: 2/2 automation steps, 8565 ms, runner exit 0)
- Final scoped formatting/lint and git diff whitespace checks: passed
- Physical Bluetooth/headset hardware and API 24/25 devices were not available for validation

Reusable device test:
`android/helpers/test_launcher_media_controls.ps1 -Install -Serial emulator-5554`
Pass `-AudioFile` to use a different local MP3 longer than 20 seconds
The test generates its own MIDI and CD fixtures and stages the MP3 in app-private files
