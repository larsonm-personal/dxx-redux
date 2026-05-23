# Plan: Music Import UX + Individual Files + MIDI Preview

Three features:
(A) Auto-import audio from game file picker instead of toast
(B) Individual audio file import with copy/reference choice + add-to-existing-set
(C) MIDI preview player with D1/D2/mission enumeration

## Decisions

- Individual files: user chooses add-to-existing-set or create-new. Checkbox "Copy file to app storage" (default checked)
- If not copied, reference by SAF URI with persistable permission. At writeM3U time, stage referenced files to temp dir
- Game file import audio detection: present add-to-set dialog inline instead of toast. Auto-switch music_mode to "files"
- MIDI: standalone player using TSF+TML, NOT using PHYSFS. Read HMP files directly from filesystem (game data is already on disk in filesDir). Write a standalone hmp2mid_mem() that reads from memory buffer
- MIDI scope: D1 + D2 built-in music PLUS mission mods with HMP files
- HOG reading: simple standalone reader (3-byte magic "DHF", entries: 13-byte name + 4-byte length + data). No PHYSFS needed since we know the file locations on disk

## Phase 1: Individual Audio File Import + Game Picker Auto-Import

### 1A: Richer add-to-set dialog [done]
- Replace current name-only dialog (MusicPickerPage.kt L191-230) with reusable composable:
  - Title: "Import Audio Files"
  - Dropdown: existing sets + "Create new set" (default if no sets exist)
  - Text field for name (visible only when "Create new set" selected)
  - Checkbox: "Copy to app storage" (default checked)
- Extract as `AddToSetDialog` composable, used by both music tab and game file picker

### 1B: Extend AudioSet for referenced files [done]
- Add referencedUris: Map<String, String> to AudioSet data class (filename -> content URI string)
- Take persistable URI permission on referenced files
- writeM3U(): for referenced files, copy to staging dir at launch time
- Persist referencedUris in JSON alongside files list

### 1C: addFilesToSet method [done]
- CustomAudioSetManager: addFilesToSet(id, newFiles, newRefs, newTrackNames)
- Appends to existing set's file list and referenced URIs
- importAudioFiles() updated to accept optional existingSetId and copyToStorage flag

### 1D: Set removal with copy/reference distinction [done]
- Copied files: delete as now (deleteRecursively)
- Referenced files: don't delete originals, remove from set only
- Update delete confirmation dialog to say "Remove" instead of "Delete" when only referenced files
- If mix: "X copied files will be deleted. Y referenced files will be unlinked"

### 1E: Game file picker audio auto-import [done]
- SetupActivity: when mp3/ogg/flac detected in unhandledFiles, collect URIs
- Show AddToSetDialog (same composable from 1A) instead of toast
- After import, set music_mode pref to "files"
- Similarly for audio-only archives

## Phase 2: MIDI Preview Player

### 2A: Native MIDI preview (midi_preview.c) [done]
- Mirrors cd_preview.c architecture: OpenSL ES output, render thread, ring buffer
- Loads gm.sf2 from AAssetManager (passed via JNI init)
- Standalone hmp2mid_mem(): reads HMP from memory buffer, outputs MIDI bytes
- Standalone HOG reader: read_hog_entry() opens .hog, scans for filename, returns malloc'd buffer
- API: init(AAssetManager*)/start(data,len,is_hmp)/stop/pause/resume/seek/getState
- Header: midi_preview.h

### 2B: Native HOG+HMP enumeration (midi_enumeration.c) [done]
- Standalone HOG scanner: opens .hog file, enumerates all entries by extension
- For each .hmp: extract -> hmp2mid_mem -> tml_load_memory -> compute duration
- Also scan for standalone .hmp/.mid files in missions/ dir
- Parse .msn/.mn2 for mission names (simple text parse, find mission_name= line)
- Return structured data to Kotlin via JNI
- Header: midi_enumeration.h

### 2C: Kotlin bridges [done]
- MidiPreviewBridge.kt: init(context)/start(data:ByteArray, isHmp:Boolean)/stop/pause/resume/seek/getState
- MidiEnumerationBridge.kt: enumerateSongs(filesDir:String) -> structured JSON string parsed on Kotlin side

### 2D: MIDI tab UI [done]
- Replace placeholder MidiSection() with full UI
- Source selector: checkboxes for D1 / D2 (if installed)
- Dropdown: mission/game selector (built-in + addons that have HMP files)
- Track list with tap-to-preview
- Track detail dialog: filename, source, duration, play/pause/stop/seek/progress
  (same mini player pattern as CdTrackDetailDialog)

## Phase 3: Build and Verify

### 3A: CMake + build + lint [done]
- Add midi_preview.c, midi_enumeration.c, jni_midi_preview.c to both d1 and d2 targets
- Link OpenSLES (already linked), TSF headers, TML headers
- Build: gradlew assembleDebug
- Lint: run-code-quality.ps1 --fix
- Fix compiler warnings

### 3B: Manual testing [pending]
- Verify audio file import flow (new set, add to existing, copy vs reference)
- Verify game picker auto-import
- Verify MIDI preview playback
- Kill emulator zombie processes if needed
