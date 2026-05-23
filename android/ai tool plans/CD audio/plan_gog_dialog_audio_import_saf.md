# Plan: GOG Dialog Layout + Audio Auto-Import + SAF BIN/CUE

Three fixes to improve the file import UX in the launcher:

## Phase 1: GOG import dialog layout fix [DONE]
**Problem**: The "Include CD audio" checkbox and "Extract" button are inside the
scrollable Column in GogImportDialog. On small screens they scroll off-screen and
the user may never see them.

**Fix**: Restructure the AlertDialog so:
- The file listing stays in a scrollable Column inside `text`
- The checkbox + extract button + done button go below the scroll area
- Add a HorizontalDivider between the scrollable and fixed areas

Implementation:
- Move the `includeAudio` checkbox Row, the extract Button, the Done button,
  and the progress indicator OUT of the Column(verticalScroll) block
- Place them in a second Column below the scroll area, with a HorizontalDivider
- The AlertDialog `text` becomes a Column with two children: scrollable area + fixed area
- Processing indicator also stays outside (fixed at bottom)
- Files: SetupActivity.kt GogImportDialog composable (~L4495-4780)

## Phase 2: Audio-only archive auto-import [DONE]
**Problem**: When user picks a ZIP/7z containing only audio files (no game files),
we show a text "This archive contains audio files but no game files. To import music,
use the Music tab" and a dismiss button. This is unhelpful -- we should open the
AddToSetDialog directly.

**Fix**: When `zipHadAudioFiles && extracted.isEmpty()`:
1. Extract the audio files from the archive to a temp dir
2. Collect their URIs
3. Set `audioImportUris` to trigger the existing AddToSetDialog
4. Clear the zip state so the "no game files" card doesn't show

But: the audio files are already extracted to tmp dir at this point (extractZipContents
extracts game files to tmp + notes hadAudioFiles). We need to re-extract just the audio
files. Actually -- the zip extraction currently only extracts game files. The audio files
are only detected, not extracted.

Better approach: when we detect `extracted.isEmpty() && anyAudio`, run a second
extraction pass that extracts audio files from the archive to tmp, then feed those
as file URIs to the audio import flow. Or create temp content URIs.

Simplest approach: extract audio files from ZIP to tmp dir, convert to file:// Uris,
pass to audioImportUris. The importAudioFiles function handles both SAF and local URIs.

Actually even simpler: the archive URIs (`zipUris`) are still available. We can launch
the AddToSetDialog with the original archive URIs and let importAudioFiles handle the
extraction (it already supports archives via extractAudioFromArchive).

Implementation:
- In the `if (extracted.isEmpty())` block where we show the error card:
  - When `zipHadAudioFiles`: instead of showing the current message + dismiss,
    immediately set audioImportUris with the original zip URIs
  - Need to thread the original zip URIs through. Currently zipUris is local to
    the file picker callback. Save them to state.
- Add `var zipAudioArchiveUris by remember { ... }` state
- When audio-only archive detected, set both the display and the import trigger
- Files: SetupActivity.kt (~L2798)

## Phase 3: SAF BIN/CUE references [DONE]
**Problem**: When BIN/CUE files are imported via the SAF file picker, the large BIN
files are copied to app storage (DiscImportDialog copies them). For files >600MB each
this is wasteful. They should be referenced in-place via SAF URIs.

**Fix**: Three sub-changes:

### 3A: Add SAF URI fields to AudioSource
- Add `binContentUri: String? = null` and `cueContentUri: String? = null` to AudioSource
- Persist/load in JSON
- When non-null, these are the primary access paths; binPaths/cuePath become display-only

### 3B: Modify DiscImportDialog "Add as Audio Source"
- When BIN/CUE were chosen via SAF picker, don't copy BIN files
- Take persistable URI permissions on BIN and CUE URIs
- Copy CUE to app storage (small file, needed for track parsing)
- Store SAF content URIs in AudioSource
- Still register with AudioSourceManager as before

### 3C: Add fd-based playback to cd_preview
- Add `cd_preview_start_fd(int fd, const char *cue_path, int audio_track, int rate)`
  that uses `fdopen(dup(fd), "rb")` instead of `fopen(path, "rb")`
- Add JNI bridge `nativeStartFd` in jni_cd_preview.c
- Add `startFd(fd, cuePath, audioTrack, sampleRate)` in CdPreviewBridge.kt
- Update CdTrackDetailDialog to check for `binContentUri` and use fd path

### 3D: Update remove dialog
- When source has SAF URIs, show "Source reference will be removed. Original files
  on external storage are not affected" instead of "files will be deleted"
- Don't attempt to delete BIN/CUE files that aren't in app storage
- Release persistable URI permissions on removal

Files: AudioSourceManager.kt, SetupActivity.kt (DiscImportDialog), cd_preview.c,
cd_preview.h, jni_cd_preview.c, CdPreviewBridge.kt, MusicPickerPage.kt

## Phase 4: Build and lint [DONE]
- gradlew assembleDebug
- run-code-quality.ps1 --fix
- Fix any warnings/errors

## Phase 5: Test [PARTIAL]
- Install APK on emulator: DONE (installs and launches)
- Game reaches menu: DONE (automation ran 13/34 steps before emulator crash)
- Emulator too unstable with swiftshader to complete full gameplay test
- GOG dialog layout, audio auto-import, SAF import: need manual testing on device
