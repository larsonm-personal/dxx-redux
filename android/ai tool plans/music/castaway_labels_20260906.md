# Castaway music labels

- [x] Verify README credits, OGG tags, hint file, and bundled fingerprints
- [x] Trace runtime naming and cache publication and fix the confirmed failure
- [x] Run scoped quality, relevant builds, and regression checks

## Findings

- `castaway_redux.zip` contains `castaway readme.pdf`, `castaway.hog`, and `castaway.mn2`
- The existing `game_data/mission_files/castaway_redux.tracklist.json` already credits all ten included OGG tracks correctly against the PDF's Music Credits page (page 3)
- The PDF also credits "Oceans Fray" for the secret level, explicitly XL only; it is not present in this Redux archive and must not be added as an eleventh fingerprint
- All ten OGG files contain title/artist comments; the launcher already reads compressed metadata through TagLib
- `mission_music_names_load` loaded a generated sidecar plus a MIDI-only embedded fallback, so OGG/MP3/FLAC tags were invisible in the in-game track label and picker without that sidecar
- Baseline emulator regression reproduced `music.name = bntrng01.ogg` instead of Crescent with precomputation paused
- The shared D1/D2 Android naming path now reads compressed tags through the existing PhysFS TagLib reader, preserving sidecar priority and MIDI fallback behavior

## Fingerprint audit

- Forced a fresh Castaway extraction/fingerprint run with `fingerprint_mission_zip_music.ps1 -Zip castaway_redux.zip -SkipBuild -SkipAcoustId -Force -OutputRoot temp/castaway-labels/fingerprints`
- All ten freshly generated fingerprints and durations exactly match the checked-in Castaway album; tracklist names have the expected artist/title credits
- Read the installed emulator-5554 APK: its `known_albums.jsonc` is identical to this checkout
- Castaway occupies unique flattened entries 499-508 of 550, below the native 1024-entry limit; `fingerprintTrackName` prioritizes its reviewed tracklist names
- The emulator's per-source fingerprint cache had no Castaway entries before testing. This cache is separate from the bundled recognition database. Background music jobs wait for that mission's route jobs to become terminal; host regeneration does not populate the device cache
- No hint or database regeneration change is required

## Validation

- Android `:app:assembleDebug` passed, including D1/D2 CMake builds for arm64-v8a, armeabi-v7a, and x86_64; no compiler warnings
- `FingerprintMatchingConfigTest` and `MissionZipMusicNamesTest` passed
- Scoped mixed-language code quality passed
- Extended `test_compressed_music_metadata.jsonc` to launch Castaway with precomputation paused and assert the current name, picker names, and name after switching tracks
- Scratch logs and fresh fingerprint output are under `temp/castaway-labels`

- Patched emulator-5554 APK passed all 26 integration steps, including Crescent in the current label and picker, Emperor Unknown and Eon Forged in the picker, and Emperor Unknown after switching tracks
