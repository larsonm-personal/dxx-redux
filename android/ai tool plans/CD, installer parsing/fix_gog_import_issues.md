# Fix GOG Import Issues (4 items)

## Issue 1: .gog/.inst import from file picker crashes
- **Symptom**: Shows import track list window briefly, then disappears. No crash report. After reopen, .gog/.inst shown in files list but nothing else extracted.
- **Root cause**: Uncertain - could be CUE parsing issue, SAF URI expiration, or Compose error. The code path looks correct in theory.
- **Fix**: 
  - Add comprehensive try/catch in DiscImportDialog LaunchedEffect composition
  - Add Log.e calls throughout for debugging
  - Ensure onDismiss always calls onRefresh so any partial imports are visible
  - Make the "Add as Audio Source" flow work independently of data track existence (it already does, but ensure robustness)
- **Status**: [x] DONE

## Issue 2: GOG extract progress bar not updating
- **Symptom**: .exe extraction shows no progress updates. Progress bar stays at 0%, then jumps to done.
- **Root cause**: `inno_extract_file()` only calls progress callback at start (0, total) and end (total, total). Decompression (the slow part) has no intermediate progress.
- **Fix**:
  - Pass progress callback through `decompress_chunk()` in inno_reader.c
  - Report decompression progress based on bytes consumed during zlib/LZMA loops
  - Added overall progress tracking (cumulative bytes across files) in jni_gog_import.c
- **Status**: [x] DONE

## Issue 3: D2 files list shows "missing" until app restart after GOG extract
- **Symptom**: After extracting via GOG import dialog, file status list still shows "missing" for extracted files.
- **Root cause**: The "Close" button in GogImportDialog calls onDismiss which does NOT refresh file list. Only "Done" calls onRefresh. Same issue in DiscImportDialog.
- **Fix**: Make onDismiss ALSO call onRefresh() for both GogImportDialog and DiscImportDialog invocations
- **Status**: [x] DONE

## Issue 4: Prune stale files from /sdcard/Download/
- **Symptom**: Files in /sdcard/Download/ on device that are no longer in host's download/ folder remain.
- **Fix**: Add pruning section after download/ push in push_game_data.sh, using `adb shell ls` and `adb shell rm` (no run-as needed for /sdcard/)
- **Status**: [x] DONE

## Build
- **Status**: [x] DONE
