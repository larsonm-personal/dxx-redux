# Plan: Music picker fixes and launcher improvements

## Task 1: Show all audio tracks with decoded names -- DONE
- MusicControlPanel.kt loadTracks() calls nativeGetNumAudioTracks/nativeGetTrackName
- These already filter data tracks (type==1 only in RBAGetNumAudioTracks)
- Track indices are 0-based in the loop, displayed as "1. Name"
- Tracks 1-2 on physical CDs are title/credits music, not level tracks
- BUT: the user says tracks 1-2 aren't usable on "basic descent disks"
- D2 track layout: track 1=DATA, track 2=title, track 3=credits, tracks 4+=level music
- After data track filtering, the panel shows audio tracks starting at index 0
- The user wants to omit the first 2 audio tracks (title + credits) from the picker
- Fix: in loadTracks(), skip the first 2 tracks (title/credits), start from index 2

## Task 2: Add scroll indicators -- DONE
- MusicControlPanel already has scroll via touch drag (touchStartY, scrollOffset, maxScroll)
- maxScroll() computes contentHeight - viewHeight correctly
- The panel uses Canvas-based drawing, not Compose
- Issue may be that maxScroll returns 0 or negative when tracks fit
- Need to verify: 8 tracks * rowHeight vs panel height
- Likely the panel rect doesn't leave enough room, or there's a clamping bug
- Actually, looking at the code, scrolling IS implemented. Let me check if tracks beyond 8 are even loaded.
- nativeGetNumAudioTracks returns count of audio-only tracks
- GOG D2 has tracks 1 (data) + tracks 2-11 (10 audio) = 10 audio tracks
- So loadTracks should get 10 entries, but the panel only shows 8 visible rows
- Scrolling should work via touch drag. Need to verify the maxScroll calculation.
- Wait - the user said "track 9" exists but they can't scroll to it. So the tracks are loaded but scroll isn't working or isn't obvious. The panel uses touch dragging which requires long press and drag.
- Add scroll indicator arrows at top/bottom of track list when more content exists.

## Task 3: Recognize .gog/.inst in set directory -- DONE  
- MusicInfoSection calls checkFiles(filesDir, MUSIC_FILES) -- checks app root
- But .gog/.inst files live in sets/default/ (the setDir)
- D2 game files use checkFiles(setDir, D2_FILES)
- Fix: pass setDir to MusicInfoSection and also check there
- Additionally, the C engine's android_apply_initial_defaults() checks filesDir root
  for PHYSFSX_exists("descent_ii.gog"), but with file sets, files are in sets/default/
- The engine uses PhysFS which has its own search path; if the set dir is on the
  search path, the engine finds them. The launcher just needs to report correctly.

## Task 4: Fix text gog/inst -- DONE
- MusicPickerPage.kt CdAudioSection text says "bin/cue or gog"
- Change to "bin/cue or gog/inst"
- Also MusicInfoSection in SetupActivity.kt says the same

## Task 5: Split game_data_to_copy_to_emulator -- DONE
- Create data/ and download/ subdirs
- Update push_game_data.sh to handle both: data/ -> sets/default, download/ -> /sdcard/Download
- Add README to game_data_to_copy_to_emulator/
- Update .gitignore: remove game_data_to_copy_to_emulator/ line, add gitignore for subdir contents
- Actually: the user wants to gitignore subfolders but NOT the parent. And add a README.
- Current .gitignore has: game_data_to_copy_to_emulator/
- Change to: game_data_to_copy_to_emulator/data/, game_data_to_copy_to_emulator/download/
- Keep the README tracked

## Task 6: Move Music button to MusicInfoSection -- DONE
- Remove "Music" OutlinedButton from ControllerSection
- Remove onEditMusic param from ControllerSection
- In MusicInfoSection, show music mode + [Edit] button
- Format: "Music [CD Audio] [Edit]" where [Edit] opens MusicPickerPage
- Need to pass onEditMusic callback to MusicInfoSection
- The mode label should reflect SharedPreferences music_mode value
