# ZIP Music Metadata Browser Study - 2026-06-09

## Goal
Study how mission ZIPs that contain music can expose those tracks from the level metadata view, reuse the existing track browser for preview playback, and align chromaprint lookup caching with existing imported-track behavior.

## Plan
- [x] Locate existing track browser UI, playback, chromaprint lookup, and cache code.
- [x] Trace mission ZIP inspection/import and level metadata navigation paths.
- [x] Inspect the mission ZIP corpus for representative audio-containing archives and edge cases.
- [x] Propose data model, temp extraction, cache ownership, and UI flow.
- [x] Define implementation phases, tests, and demo ZIPs.

## Notes
- Do not reimplement track browsing or preview playback if the existing browser can be reused.
- ZIP-contained tracks may be ephemeral, so cache keys should be based on stable archive/member identity and content hashes rather than temp file paths.
- Large ZIP handling may need a separate temp-file extraction path from small in-memory analysis.

## Existing Pieces

### Track Browser And Preview
- `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt`
  - `TrackPreviewDialog` is the existing CD/custom-audio track list browser.
  - `MidiSection` plus `MidiTrackPreviewDialog` is the existing MIDI/HMP browser and preview path.
  - `CdTrackDetailDialog` previews CD image tracks through `CdPreviewBridge`.
  - `AudioFileDetailDialog` previews imported MP3/OGG/FLAC files through `MediaPlayer`.
  - `importAudioFiles` already extracts audio from ZIP/DXA/7z when importing custom audio sets and fingerprints imported audio files.
- `CdPreviewBridge.kt`, `MidiPreviewBridge.kt`, and `MidiEnumerationBridge.kt` are the preview/enumeration JNI bridges.
- `android/app/src/main/cpp/shared/midi_enumeration.c` currently enumerates HMP/MID tracks from built-in HOGs and mission HOGs in a real directory, not directly from mission ZIPs.

### Fingerprint And Lookup
- `FingerprintBridge.kt` can:
  - fingerprint MP3/OGG/FLAC from a filesystem path,
  - fingerprint CD tracks from BIN/CUE data,
  - match generated chromaprints against bundled `known_discs.json5`.
- `AcoustIdClient.kt` can do optional web lookup when an API key is present, with rate limiting and retry.
- Imported custom audio stores matched names in `CustomAudioSetManager.AudioSet.trackNames`, `trackConfidences`, and `trackNumbers`, persisted in `custom_audio_sets.json`.
- There is no general loose-audio fingerprint cache yet. Imported audio folds the result into the custom audio set record.

### Mission ZIP And Metadata UI
- `MissionZip.kt` scans ZIP constituents and chooses `stored_zip` vs `extracted_bundle` based on `SMALL_IN_MEMORY_LIMIT_BYTES`.
- `ModManager.kt` stores mission ZIPs in `filesDir/mods`, exposes `ModDetails.missionZip`, and stages mission ZIP contents for game launch under `.generated_mission_zips`.
- `ModManager.hasEnabledMissionZipBuiltinMusic` already detects built-in mission music by checking:
  - top-level `.sng`,
  - nested `.dxa` entries,
  - music entries inside `.hog`.
- `SetupSections.kt` owns the mod details dialog, the level metadata buttons, and `LevelMetadataDialog`.
- `LevelMetadata.kt` stages selected ZIP level entries into `cacheDir/level_metadata/<uuid>/staged` for native analysis.

## Corpus Findings
- Current `game_data/mission_files` contains 104 ZIP files.
- A later focused check of `trine2.zip` showed an important layout that was missed by top-level ZIP scanning: the custom OGG soundtrack is embedded inside `trine2.hog`.
- Top-level `.sng` files were found in:
  - `Chasm.zip`
  - `EAF2.zip`
  - `grad3d.zip`
  - `kcxf2.zip`
  - `Lostlvls.zip`
  - `Mandrill.zip`
  - `Obsidian.zip`
  - `ORION-D2.zip`
  - `TEW.zip`
  - `TheOmicronProject.zip`
- Only one outer ZIP currently contains a `.dxa`: `levigen.zip`.
- The current corpus scan did not find top-level loose `.ogg`, `.mp3`, `.flac`, `.wav`, `.mid`, `.midi`, `.hmp`, or `.hmq` files outside `.sng`; however, several large ZIPs contain compressed audio inside HOG files.
- Large-HOG soundtrack examples:
  - `trine2.zip` -> `trine2.hog`: `descent.sng` plus 14 OGG tracks, about 48.9 MB of audio, 96.3 percent of HOG payload.
  - `Trine1.zip` -> `trine1.hog`: 15 audio-like tracks, about 58.8 MB.
  - `cererian_1.3.zip` -> `cererian.hog`: 28 audio-like tracks, about 61.7 MB, including large OGG tracks.
  - `U3AAH.zip` -> `U3AAH.hog`: 3 OGG tracks, about 47.8 MB.
- Existing tests already synthesize a nested DXA with `descent.sng` plus `Uneasy4.ogg` in `ModManagerMissionZipTest`.

## Proposed Architecture

### Core Model
Add a ZIP music scanner that produces a reusable, UI-neutral model:

```kotlin
data class MissionZipMusicCatalog(
    val archivePath: String,
    val archiveIdentity: MissionZipArchiveIdentity,
    val sources: List<MissionZipMusicSource>,
)

data class MissionZipMusicSource(
    val id: String,
    val label: String,
    val containerPath: String,
    val tracks: List<MissionZipMusicTrack>,
)

data class MissionZipMusicTrack(
    val id: String,
    val displayName: String,
    val archiveEntryPath: String,
    val nestedEntryPath: String? = null,
    val hogEntryName: String? = null,
    val kind: String, // midi, compressed_audio, song_reference
    val extension: String,
    val sizeBytes: Long,
    val contentSha256: String? = null,
    val matchedName: String? = null,
    val confidence: Float? = null,
    val trackNum: Int? = null,
    val lookupName: String? = null,
)
```

`archiveIdentity` should be stable across app restarts and independent of temp files:
- archive filename,
- archive length,
- archive last modified time,
- optionally archive SHA-256 when lookup/cache work starts.

Track identity should include:
- archive identity,
- outer entry path,
- nested DXA/HOG path if any,
- HOG entry name if any,
- content SHA-256 once extracted or streamed.

### Scanner
Create `MissionZipMusic.kt` with:
- `inspectMusic(zipFile: File): MissionZipMusicCatalog?`
- `hasMusic(scan/catalog)` for UI button visibility
- helpers to parse `.sng` rows and identify referenced filenames
- outer ZIP scan for `.sng`, loose HMP/MID/HMQ, loose OGG/MP3/FLAC/WAV
- nested DXA scan using `ZipInputStream`, matching the existing `ModManager.dxaHasBuiltinMusic`
- HOG scan for HMP/MID/HMQ and compressed OGG/MP3/FLAC audio, reusing the simple HOG reader pattern already in Kotlin/C tests where possible

Important behavior:
- `.sng` is not itself playable, but it gives ordering and references. If referenced files are present in the same ZIP/DXA/HOG, use that order. If not, show song-list references as unavailable rows or hide them behind a note, depending on UX preference.
- HOG-contained `descent.sng` must be parsed. `trine2.hog` uses this exact shape: `descent.sng` lists `descent.ogg`, `briefing.ogg`, `endlevel.ogg`, `endgame.ogg`, `credits.ogg`, and `game01.ogg` through `game09.ogg`.
- HMP/HMQ/MID should use the MIDI preview path.
- OGG/MP3/FLAC/WAV should use the audio-file preview path and chromaprint path where supported.
- Chromaprint should be limited to OGG/MP3/FLAC initially because `FingerprintBridge` advertises MP3/OGG/FLAC; WAV can be preview-only unless native decoder support is confirmed.

### Staging And Temp Files
Add `MissionZipMusicStageManager`, probably under `cacheDir/mission_zip_music`.

Responsibilities:
- Materialize exactly one selected track or one selected source into temp files on demand.
- Use real files because `MediaPlayer` and `FingerprintBridge.fingerprintAudioFile(path)` already want paths.
- For MIDI/HMP from HOG, either:
  - read bytes directly and call `MidiPreviewBridge.start(data, isHmp, sampleRate)`, or
  - stage the containing HOG and reuse `MidiPreviewBridge.readHogEntry`.
- For compressed audio in a HOG, extract only the selected HOG member to `cacheDir/mission_zip_music/<archive-key>/<track-key>/<safe-name>`; do not stage the full 50-60 MB HOG just to preview or fingerprint one track.
- For compressed audio directly in a ZIP/DXA, stage the selected archive member to the same cache layout.
- For large `extracted_bundle` mission ZIPs, prefer the same API but allow materializing from already extracted launch/stage content if a stable extracted root exists. Do not rely on `.generated_mission_zips` for the metadata dialog because it is tied to enabled-game launch state and can be deleted/rebuilt.
- Evict by deleting old cache subdirs on app start or before staging:
  - keep current session entries,
  - delete entries older than 24 hours,
  - enforce a size cap such as 256 MB.

### Reusing The Browser
Refactor, do not fork:
- Extract the generic list/dialog shell from `TrackPreviewDialog` into a composable such as `TrackBrowserDialog`.
- Extract row model and preview detail model:
  - MIDI preview item uses bytes provider plus `MidiPreviewBridge`.
  - compressed audio preview item uses file provider plus `MediaPlayer`.
  - CD preview item keeps existing `CdPreviewBridge`.
- Keep the current Music page behavior by adapting `AudioSourceManager` and `CustomAudioSetManager` into the new row model.
- Add a mission-ZIP adapter that maps `MissionZipMusicCatalog` into the same row model.

Recommended split:
- `TrackBrowser.kt`: shared browser UI and common rows.
- `TrackPreviewDialogs.kt`: MIDI, CD, file detail dialogs.
- Keep `MusicPickerPage.kt` focused on selecting configured music modes.

### UI Flow
In `SetupSections.ModDetailsDialog`:
- Compute a `MissionZipMusicCatalog` alongside `topLevelMetadataTargets` when `details.missionZip != null`.
- If one or more playable tracks are present, show a button near the level metadata button:
  - label: `Music tracks`
  - single source: open browser directly.
  - multiple sources: browser can group by source label, so no extra pre-picker is needed unless the list is huge.
- Do not put music controls inside `LevelMetadataDialog`; the user asked for this "for the metadata view", but the existing level metadata entry point is in the mod details dialog. Keeping the music browser adjacent to level metadata avoids making the native analyzer aware of music.

### Use Mission Soundtrack Preference
Add an explicit launcher preference for the in-game playback policy, separate from the global music source selection.

Recommended preference:
- Key: `use_mission_soundtrack_when_available`
- Storage: `dxx_prefs`
- Type: boolean for first implementation
- Default: `true`
- Export/import: include it in the launcher config import/export preference whitelist when implemented

Recommended UI location:
- `EnginePreferencesPage.kt`, in the launcher Game Preferences area.
- Add a compact music/gameplay preference row labelled `Use mission soundtrack when available`.
- Supporting text should explain the behavior without exposing implementation details, for example:
  - `Mission packs can include their own song lists and OGG/MP3/FLAC/HMP/MIDI tracks. When enabled, those tracks play instead of the global music mode for that mission.`
- Do not put the setting in `MusicPickerPage.kt` as the primary home. That page picks global music sources and imports audio sets; this setting is launch policy for mission-provided music.
- Optionally add a read-only note in `ModDetailsDialog` when a ZIP contains music:
  - `This mission includes a soundtrack. Game Preferences controls whether it overrides your global music mode.`
- Optionally add a secondary status line in `SetupSections.MusicInfoSection`, but only after the preference exists. The main editable control should remain in Game Preferences.

Launch resolution rules:
- `musicTypeOverride` continues to win. Resume/explicit launch paths that pass a music type should preserve current behavior until we intentionally redesign save/resume music policy.
- Compute `missionHasSoundtrack` from existing detection first:
  - `game != null && ModManager(filesDir).hasEnabledMissionZipBuiltinMusic(game)`
- Then compute:
  - `useMissionSoundtrack = prefs.getBoolean("use_mission_soundtrack_when_available", true)`
  - `missionZipBuiltinMusic = musicTypeOverride == null && game != null && useMissionSoundtrack && missionHasSoundtrack`
- If `missionZipBuiltinMusic` is true:
  - Write `MusicType=1` so the engine uses built-in/addon music.
  - Do not write `OrigTrackOrder`.
  - Do not write `CMLevelMusicPath`.
  - Add a concise debug/setup log line naming the selected mission soundtrack policy.
- Otherwise retain the current global mode behavior:
  - `midi` -> `MusicType=1`
  - `cd` -> `MusicType=2` plus `OrigTrackOrder=1`
  - `files` -> `MusicType=3` plus `CMLevelMusicPath`/M3U config

Why this default:
- Mission authors who ship OGG/HMP/MIDI music generally expect it to be heard with the mission.
- The current default CD mode already makes Trine 2-style mission music work for many users, so defaulting this preference to true preserves that good behavior while extending it to users who selected MIDI or custom files globally.
- Users who prefer a global soundtrack can turn the setting off once and keep their selected global music mode for every mission.

Implementation targets:
- `SetupConfigFiles.kt`: replace the current `mode == "cd"` mission-music condition with the new preference gate.
- `EnginePreferencesPage.kt`: add the toggle and persist it in `dxx_prefs`.
- `ConfigImportExport.kt`: include the setting with the other launcher/game preferences.
- Consider a small shared constants file or object for the preference key so the UI and launch code do not duplicate the string.
- Consider extracting a pure launch-policy helper from `writeMusicConfigForLaunch` so the behavior can be unit-tested without launching the game.

Tests:
- Unit-test the launch-policy helper:
  - explicit `musicTypeOverride` wins even when mission music is available.
  - preference true plus mission music selects `MusicType=1` from global `cd`.
  - preference true plus mission music selects `MusicType=1` from global `midi`.
  - preference true plus mission music selects `MusicType=1` from global `files`.
  - preference false plus global `cd` keeps `MusicType=2` and `OrigTrackOrder`.
  - preference false plus global `files` keeps `MusicType=3` and custom audio config.
  - no mission music preserves existing global mode behavior.
- UI/persistence test:
  - open Game Preferences, toggle `Use mission soundtrack when available`, leave and return, verify persisted state.
- Integration/manual demonstration:
  - Import `trine2.zip`.
  - Enable the preference, launch Descent 2 into Trine 2, and verify built-in/addon music is selected.
  - Disable the preference, choose a global MIDI or audio-files mode, launch again, and verify the global mode is preserved.

Open policy note:
- If multiple enabled mission ZIPs for the same game contain soundtracks, the current boolean detector is still enough to choose `MusicType=1`; the game launch staging and PhysFS search path determine the actual mission HOG/song list. If that order becomes confusing in practice, the mod details UI should surface which enabled mission supplies music.

### Chromaprint Cache
Add a sidecar manager separate from `CustomAudioSetManager`, for example `MissionZipAudioFingerprintCache.kt`, persisted as:
- `filesDir/mission_zip_audio_fingerprints.json`

Suggested JSON shape:

```json
{
  "schema": "dxx-mission-zip-audio-fingerprints-v1",
  "entries": [
    {
      "archive_name": "Example.zip",
      "archive_size": 123456,
      "archive_mtime": 1781012345000,
      "entry_path": "music/track01.ogg",
      "nested_path": "",
      "sha256": "content sha",
      "duration_ms": 123000,
      "chromaprint": "...",
      "local_match_name": "...",
      "local_match_confidence": 0.91,
      "local_match_disc_id": "...",
      "local_match_track": 6,
      "acoustid_name": "...",
      "lookup_at": 1781012345000,
      "lookup_status": "ok"
    }
  ]
}
```

Lookup flow:
- On browser open, load cached entries and annotate tracks.
- On explicit "Identify" or "Identify all" action:
  - stage compressed audio,
  - compute content SHA-256,
  - if matching cache entry has chromaprint/result, reuse it,
  - otherwise call `FingerprintBridge.fingerprintAudioFile(path)`,
  - match local DB through `FingerprintBridge.matchFingerprint`,
  - optionally call `AcoustIdClient.lookupFingerprint` when no local match or when user asks for web lookup.
- Cache should store the generated chromaprint and duration, not just the display name, so future AcoustID lookup can happen without re-fingerprinting.
- Cache should not use temp path as identity.

### Feedback Points
- Should `.sng` rows that reference only base game HMPs be shown as tracks? For many current mission ZIPs the `.sng` just reorders built-in HMP names, not custom audio. My recommendation: show a `Mission song list` source with referenced names, but only enable preview where the referenced content can be resolved from the ZIP/HOG/staged base data.
- Should the browser include base-game tracks referenced by `.sng`? That is useful for explaining what the mission will play, but it blurs "contained in the zip". My recommendation: first phase only preview contained tracks; second phase resolves base-track references.
- Should AcoustID lookup be automatic? My recommendation: no. Fingerprint locally on demand or once per track when opening the browser, but web lookup should be an explicit button if an API key is configured.
- Should `.wav` be chromaprinted? Only after confirming native `pcm_decoders.c` supports it through `fingerprint_from_audio_file`; otherwise preview-only.
- Should large `extracted_bundle` ZIPs share launch extraction? My recommendation: no for the first implementation. Use cache staging so the browser works even when the mod is disabled and launch staging has not been generated.
- Should mission ZIP custom soundtracks override the user's global music mode? Current launch code only auto-selects built-in/addon music when the global mode is `cd`. The recommended answer is now captured above: add `Use mission soundtrack when available` in Game Preferences, default it to true, and use it as the explicit gate for mission music regardless of global music mode.

## Implementation Phases

### Phase 0: Mission Soundtrack Launch Policy
Status: completed 2026-06-09.

- [x] Add the `use_mission_soundtrack_when_available` preference, defaulting to true.
- [x] Add the Game Preferences toggle in `EnginePreferencesPage.kt`.
- [x] Update `SetupConfigFiles.writeMusicConfigForLaunch` so mission ZIP built-in/addon music is selected when the preference is true and mission music exists, regardless of global music mode.
- [x] Preserve `musicTypeOverride` precedence for resume/explicit override paths.
- [x] Add config import/export support.
- [x] Add unit tests for launch-policy resolution.
- [ ] Add a UI/persistence test if the Compose test harness is expanded for this settings page.
- [ ] Demonstrate manually with `trine2.zip`, since it has HOG-contained OGG music and exposes the current global-mode coupling problem.

Verification:
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.MusicLaunchPolicyTest`
- `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\java\com\dxxredux\app\EnginePreferencesPage.kt','android\app\src\main\java\com\dxxredux\app\SetupConfigFiles.kt','android\app\src\main\java\com\dxxredux\app\ConfigImportExport.kt','android\app\src\test\java\com\dxxredux\app\MusicLaunchPolicyTest.kt','android\ai tool plans\music\plan_zip_music_metadata_browser_study_20260609.md','android\ai tool plans\music\plan_trine2_embedded_soundtrack_investigation_20260609.md')`

### Phase 1: Discovery And UI Button
Status: completed 2026-06-09.

- [x] Add `MissionZipMusic.kt`.
- [x] Add music roles/extensions to mission ZIP scanning without changing importability rules.
- [x] Detect outer `.sng`, outer loose music files, nested DXA `.sng` and loose compressed audio, and HOG HMP/MID/HMQ entries.
- [x] Detect HOG-contained `.sng` plus HOG-contained OGG/MP3/FLAC entries. This is required for `trine2.zip`, `Trine1.zip`, `cererian_1.3.zip`, and `U3AAH.zip`.
- [x] Add a `Music tracks` button to `ModDetailsDialog` when catalog has playable or listable tracks.
- [x] Add unit tests for top-level `.sng`, HOG HMP, nested DXA OGG, HOG-contained `.sng` plus OGG, and no-music ZIPs.

Verification:
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.MissionZipMusicTest --tests com.dxxredux.app.MusicLaunchPolicyTest`
- `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\java\com\dxxredux\app\MissionZipMusic.kt','android\app\src\main\java\com\dxxredux\app\ModManager.kt','android\app\src\main\java\com\dxxredux\app\SetupSections.kt','android\app\src\test\java\com\dxxredux\app\MissionZipMusicTest.kt','android\app\src\test\java\com\dxxredux\app\MusicLaunchPolicyTest.kt','android\ai tool plans\music\plan_zip_music_metadata_browser_study_20260609.md')`

### Phase 2: Browser Refactor
- Extract `TrackBrowserDialog` and preview detail dialogs from `MusicPickerPage.kt`.
- Adapt existing CD/custom-audio pages to use the extracted browser.
- Add mission-ZIP adapter for the same browser row model.
- Keep behavior identical for the existing Music tab.
- Tests:
  - Compose/unit coverage where feasible for row model generation.
  - Existing music UI tests should still pass.

### Phase 3: Temp Staging And Preview
Status: compressed-audio and MIDI preview slices completed 2026-06-09.

- [x] Add `MissionZipMusicStageManager`.
- [x] Implement compressed audio staging to cache and preview through a shared `MediaPlayer` detail dialog.
- [x] Extract the custom-audio local file player into reusable `AudioFilePreviewDialog`.
- [x] Add mission ZIP `Preview` actions for playable compressed audio tracks.
- [x] Add staging tests for top-level audio, nested DXA audio, and HOG-contained audio.
- [x] Extract the MIDI/HMP preview shell into reusable `MidiBytesPreviewDialog`.
- [x] Implement HMP/HMQ/MID preview from selected mission ZIP track bytes.
- [x] Add byte extraction tests for top-level MIDI, nested DXA HMP, and HOG-contained HMQ.
- [x] Implement OGG/MP3/FLAC/WAV extraction from a HOG member to a temp file for `MediaPlayer`.
- Reuse staged OGG/MP3/FLAC files for `FingerprintBridge` when the chromaprint cache phase begins.
- [x] Add cleanup policy.
- Automation:
  - open mod details,
  - open music tracks,
  - play first MIDI or OGG track,
  - introspect `music_preview` state or add a setup introspection field for file-preview player state.

Verification:
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.MissionZipMusicTest --tests com.dxxredux.app.MissionZipMusicStageManagerTest --tests com.dxxredux.app.MusicLaunchPolicyTest`
- `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\java\com\dxxredux\app\AudioFilePreviewDialog.kt','android\app\src\main\java\com\dxxredux\app\MissionZipMusicStageManager.kt','android\app\src\main\java\com\dxxredux\app\MusicPickerPage.kt','android\app\src\main\java\com\dxxredux\app\SetupSections.kt','android\app\src\test\java\com\dxxredux\app\MissionZipMusicStageManagerTest.kt','android\app\src\test\java\com\dxxredux\app\MissionZipMusicTest.kt','android\ai tool plans\music\plan_zip_music_metadata_browser_study_20260609.md')`

### Phase 4: Fingerprint Cache
- Add `MissionZipAudioFingerprintCache.kt`.
- Add local chromaprint action for OGG/MP3/FLAC.
- Store content SHA-256, chromaprint, duration, local match, and optional web lookup result.
- Reuse `FingerprintBridge` and `AcoustIdClient`; do not add a new fingerprint implementation.
- Unit tests:
  - cache key survives temp path changes,
  - cache invalidates when archive size/mtime or content hash changes,
  - cached result annotates browser rows.

### Phase 5: AcoustID Lookup UX
- Add explicit per-track and "Identify all" actions, enabled only when `AcoustIdClient.configure(context)` succeeds.
- Use existing rate limiting.
- Cache failed/empty lookup status with timestamp to avoid repeated accidental lookups.
- Add user-visible status for "local match", "web match", "no match", and "lookup failed".

### Phase 6: Large ZIP Path
- Exercise a mission ZIP over `SMALL_IN_MEMORY_LIMIT_BYTES`.
- Confirm scanner uses `ZipFile` streaming and stage-on-demand, not full memory extraction.
- Add a regression fixture or synthetic large ZIP test that verifies only selected music entries are extracted.
- Add a real-file manual/regression demonstration with `trine2.zip`: open music browser, list 14 OGG tracks in song-list order, preview one level track, fingerprint one OGG, and launch the mission to verify the game enters built-in/addon music mode and plays a HOG-contained OGG.

## Demonstration ZIPs
- `Obsidian.zip`: top-level `.sng`, HMP names, existing test pattern for HMP in HOG.
- `Chasm.zip`: simple top-level `.sng`.
- `TheOmicronProject.zip`: `.sng` in a subdirectory, useful for rooted path behavior.
- `levigen.zip`: only current corpus member with nested `.dxa`; useful for nested archive handling even if it may not contain music.
- `trine2.zip`: required real-world HOG-contained OGG soundtrack case. The custom soundtrack is not visible from top-level ZIP scanning.
- `Trine1.zip`, `cererian_1.3.zip`, and `U3AAH.zip`: additional large HOG-contained OGG soundtrack cases.
- Synthetic fixture based on `createMissionZipWithDxaMusic()` in `ModManagerMissionZipTest`: nested DXA with `descent.sng` and `Uneasy4.ogg`, required for compressed-audio preview and chromaprint tests.
- Future real pack with top-level or nested `.mp3/.ogg/.flac` should be added to `game_data/mission_files` once found.
