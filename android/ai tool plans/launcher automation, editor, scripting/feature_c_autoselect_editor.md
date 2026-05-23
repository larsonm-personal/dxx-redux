# Feature C: Launcher-based Autoselect Editor -- Completion Notes

## Status: COMPLETE

## What was built
- Compose UI page for reordering weapon autoselect priority (drag-to-reorder)
- JNI C++ layer for reading/writing weapon ordering from pilot files
- D1: text .plx format (searches for .plx files directly since .plr may not exist)
- D2: binary .plr format (interleaved primary/secondary bytes at computed offset)
- Integration test verifying file format handling

## Files Created
- `android/app/src/main/cpp/android_autoselect.cpp` -- JNI C++ for read/write
- `android/app/src/main/java/com/dxxredux/app/NativeAutoselectPatcher.kt` -- JNI wrapper
- `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt` -- Compose UI
- `android/tests/test_autoselect_plx.ps1` -- Integration test (16 checks)

## Files Modified
- `android/app/src/main/cpp/CMakeLists.txt` -- added android_autoselect.cpp to D1+D2 targets
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` -- nav state, page routing, button

## Key Design Decisions
- D1 searches for .plx files (not .plr) because D1 stores weapon ordering in .plx text files and .plr may not exist yet
- D2 searches for .plr files (binary format with interleaved weapon ordering bytes)
- Weapon names are hardcoded English strings matching the in-game text.h macros exactly
- SAVE_FILE_ID and COMPATIBLE_PLAYER_FILE_VERSION are duplicated locally (they're private #defines in playsave.c)
- Icons.Filled.Menu used instead of DragHandle (only material-icons-core is available, not extended)
- No changes to d1/ or d2/ source -- desktop builds unaffected
