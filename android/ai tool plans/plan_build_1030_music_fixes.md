# Plan: Build 1030 Music System Fixes

Issues found during build #1030 testing. 4 phases ordered by severity.

Decisions:
- Config writes: write to all three descent.cfg files (root + d1x-redux/ + d2x-redux/ if they exist)
- GOG path fix: store correct relative path from filesDir (e.g., "sets/MySet/descent_ii.gog")
- Source removal: delete extracted files from app data dir (BIN/CUE, GOG audio, custom audio sets). Warn user before delete. Rationale: prevents orphaned files; original source archives still exist for re-import
- 7z: support 7z everywhere zip is supported. Use Apache Commons Compress (pure Java). Music packs are distributed as 7z
- Pilot files: do NOT store music prefs. MusicType is global in descent.cfg. Item #5 root cause is config path bug (1A)

## Phase 1: Critical Bug Fixes

### 1A: descent.cfg music mode writes go to wrong location -- [x]

- `writeMusicConfigForLaunch()` (SetupActivity.kt L3484) writes `filesDir/descent.cfg`
- `enableRedbookInConfig()` (SetupActivity.kt L3406) same
- D2 engine reads `filesDir/d2x-redux/descent.cfg` (d2/misc/physfsx.c L59)
- D1 engine reads `filesDir/d1x-redux/descent.cfg`
- After first game run creates game-specific config, launcher music changes are silently ignored
- Root cause of items #5 and #9.1

Fix: extract `updateConfigFiles(filesDir, settings: List<Pair<String,String>>)` helper that writes to:
1. `filesDir/descent.cfg` (fallback for first launch)
2. `filesDir/d1x-redux/descent.cfg` (if dir exists)
3. `filesDir/d2x-redux/descent.cfg` (if dir exists)

Refactor `enableRedbookInConfig` and `writeMusicConfigForLaunch` to use it.

### 1B: GOG CD audio tracks don't play in preview player -- [x]

- `registerGogAudioSource()` stores bare filenames ("descent_ii.gog")
- GOG files extracted to setDir (e.g. `filesDir/sets/SetName/`)
- Preview player resolves `File(filesDir, "descent_ii.gog")` = wrong path
- Fix: store path relative to filesDir: `"sets/SetName/descent_ii.gog"`

In registerGogAudioSource(), change:
```kotlin
val relBase = setDir.toRelativeString(filesDir) + File.separator + base
cuePath = "$relBase.inst"
binPaths = listOf("$relBase.gog")
```

### 1C: GOG .exe auto-set CD audio condition -- [x]

- At L4408: condition is `includeAudio && findGogPair(setDir) != null`
- Verify includeAudio checkbox defaults on when audio files detected
- With 1A fixed, enableRedbookInConfig will write to correct locations
- May already work correctly; verify during testing

## Phase 2: Import Warning Improvements

### 2A: .gog/.inst unpaired warning -- [x]

- Importing lone .gog or lone .inst gives no warning (silently treated as game file)
- .bin without .cue correctly warns
- Fix: add parallel warning logic after L1773 for unpaired .gog/.inst
- "descent_ii.gog requires a matching .inst file" / vice versa

### 2B: More comprehensive import warning messages -- [x]

- Catch-all unhandledFiles warning just lists filenames (L1771-1776)
- Fix: categorize unhandled files:
  - Audio (.mp3/.ogg/.flac) -> "Audio files can be added via the Music tab"
  - Unknown -> "File type not recognized: <name>"

### 2C: ZIP/7z with only music -> prompt -- [x]

- extractZipContents() only looks for ALL_GAME_FILENAMES (L1524)
- Music-only ZIP/7z shows "No game files found in ZIP archive"
- Fix: when no game files found, scan entries for audio extensions
- If audio found: "This archive contains audio files but no game files. To import music, use the Music tab"

### 2D: Add 7z support alongside ZIP -- [x]

Current state:
- ARCHIVE_EXTENSIONS includes "7z" (MusicPickerPage.kt L1004)
- File picker accepts "application/x-7z-compressed"
- extractAudioFromArchive() uses ZipInputStream only -- 7z silently fails
- SetupActivity game file import only handles .zip

Fix:
- Add Apache Commons Compress dependency (pure Java, supports 7z)
- In MusicPickerPage.kt extractAudioFromArchive(): detect 7z by extension, use SevenZFile
- In SetupActivity.kt: accept .7z in file picker, add extract7zContents() parallel to extractZipContents()
- Pin commons-compress to specific version in dependency_base.txt / build.gradle

## Phase 3: UI Polish

### 3A: Preview player progress bar immediately -- [x]

- Slider only renders when durationMs > 0 (MusicPickerPage.kt ~L962)
- Fix: show disabled slider at value=0 immediately, enable when duration known

### 3B: Center scroll up/down icons -- [x]

- ScrollArrows uses Alignment.TopEnd / BottomEnd (right-aligned)
- Fix: change to TopCenter / BottomCenter

### 3C: Source info show full paths -- [x]

- CD audio source info dialog shows relative paths (MusicPickerPage.kt ~L329-330)
- Fix: resolve to absolute: `File(filesDir, src.cuePath).absolutePath`

### 3D: Source removal deletes extracted app-dir files -- [x]

Current behavior:
- CD audio removeSource(): only removes from JSON, files left on disk
- Custom audio removeSet(deleteFiles=true): deletes files via deleteRecursively()

New behavior for ALL source types:
- Check if source files are inside filesDir (app data dir)
- If yes: delete files and show warning "Extracted files will be deleted. Re-import from original source to restore"
- If no (SAF/external): keep current "files will remain on disk" message

CD audio removal changes:
- After removeSource(), if BIN/CUE paths resolve inside filesDir, delete those files
- Show confirmation dialog with delete warning

Custom audio:
- Already deletes files (deleteFiles=true) -- keep existing behavior
- Update messaging to match: "X audio files will be deleted"

## Phase 4: New Features

### 4A: Audio file track info player -- [x]

- CD track detail dialog has play/pause/stop + seekable progress bar
- Audio file track info dialog only shows metadata (filename, set, match info)
- Fix: add MediaPlayer-based preview player to audio file track info dialog
- Use DisposableEffect for cleanup on dialog dismiss
- Show same progress bar UI as CD track player

### 4B: GOG .exe installer messaging -- [x]

- Add explanatory text to GogImportDialog:
  - Before extraction: "Game files will be extracted to [SetName]"
  - If audio included: "CD audio will be configured as the active music source"
  - After extraction: confirm what was configured

## Verification

After each phase, run:
- Build: `cd android; .\gradlew.bat assembleDebug`
- Lint: `android\run-code-quality.ps1 --fix`

Key manual tests:
1. Change music mode in picker -> verify both d1x-redux/ and d2x-redux/ descent.cfg updated
2. Import GOG .exe with audio -> preview a GOG CD track plays
3. Import lone .gog without .inst -> warning toast
4. Import ZIP/7z of .mp3 files -> message suggests music tab
5. Remove CD audio source extracted to app dir -> files deleted with warning
6. Remove custom audio set -> files deleted with warning (existing behavior preserved)
7. CD track detail -> progress bar visible immediately
8. Scroll arrows centered
9. Audio file track info -> playback controls work
10. Import 7z music pack -> extracted and registered correctly
