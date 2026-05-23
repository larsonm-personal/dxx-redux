# Infinite Abyss reconstructed CD static research

## Goal
Research why a merged-local imported Infinite Abyss CD image plays as static in-game on regular Android while the launcher track list player plays correctly.

## Plan
- [x] Trace the import and merged CUE/BIN generation path
- [x] Compare launcher preview playback against in-game Redbook playback
- [x] Inspect PhysFS/local path handling for reconstructed files
- [x] Review relevant git history and prior plan notes
- [x] Identify likely causes and logging probes
- [x] Add exported Android Redbook diagnostics for playlist, CUE parse, and track playback
- [x] Route local reconstructed CUE playback through absolute paths to bypass PhysFS shadowing
- [x] Add a narrow regression test for playlist CUE-path selection
- [x] Validate with Android native build and targeted launcher unit test

## Notes
- This moved past pure research because a small, low-risk fix fell directly out of the CUE path mismatch investigation

## Findings
- Multi-BIN SAF imports are normalized by concatenating selected BIN files into one local BIN and writing a generated single-FILE CUE with cumulative 2352-byte sector offsets
- Launcher preview opens the generated CUE and local BIN by absolute paths, so it does not go through PhysFS search order
- In-game playback writes `audio_playlist.json` with a relative CUE path and a BIN path that is usually `/proc/self/fd/N` for local path-backed sources
- `rbaudio_bin.c` opens the CUE through PhysFS, so an active file-set CUE with the same filename can shadow the generated merged CUE in filesDir
- If a multi-FILE original CUE is parsed against the merged BIN, `rbaudio_bin.c` ignores CUE FILE directives and treats per-file INDEX values as single-image offsets, which can make it decode data sectors as 16-bit stereo PCM static
- D2 Redbook level playback can also decode a data track as audio if `OrigTrackOrder` is not set, because `REDBOOK_FIRST_LEVEL_TRACK` falls back to 1 and `RBAPlayTracks()` does not reject or skip data tracks
- Infinite Abyss is present in `known_discs.json5` without `legacy_disc_id` or track mapping, so the fallback D2-disc detection depends on `OrigTrackOrder=1` being successfully written before launch

## Suggested probes
- Log the exact generated `audio_playlist.json` entry in the game process: source label, cue value, bin value, local/SAF mode, fd path, and file size
- In `parse_source_cue()`, log the resolved CUE source and for each track: index, type, start sector, length, source index, and BIN length
- In `RBAPlayTracks()`, log first/last, first track type, `RBAGetDiscID()`, `GameCfg.OrigTrackOrder`, and whether the first track is data
- In `refill_pcm()`, log once if the current track type is data but PCM decode is about to read it
- Route these Android logs through `debug_log(DLOG_GAME, ...)` or a new audio category so they appear in exported launcher debug logs, not just logcat

## Likely fixes to evaluate
- For local reconstructed sources, write absolute CUE and BIN paths to `audio_playlist.json` and teach `rbaudio_bin.c` to open absolute CUE paths with stdio, matching the launcher preview path and bypassing PhysFS shadowing
- Alternatively put generated CUE/BIN artifacts in a reserved subdirectory and include that relative path in the playlist to avoid name collisions with active-set files
- Add a guard in `RBAPlayTracks()` or the render advance path so data tracks are skipped or rejected instead of decoded as PCM
- Add `legacy_disc_id` and explicit `track_mapping` for `descent-ii-infinite-abyss` if this disc should behave like a normal D2 CD even when `OrigTrackOrder` is absent

## Implemented this pass
- Added `debug_log(DLOG_GAME, ...)` Redbook probes in `rbaudio_bin.c` for playlist entries, CUE resolution, parsed track tables, play-range selection, and one-shot data-track decode warnings
- Changed launcher playlist writing so local path-backed sources emit an absolute CUE path, matching the launcher preview path instead of relying on PhysFS resolution
- Extended `rbaudio_bin.c` so absolute CUE paths are opened with stdio while relative paths still use PhysFS
- Added a focused Kotlin unit test covering the playlist CUE-path decision for merged local sources and SAF-backed sources

## Validation
- `android\\gradlew.bat :app:buildCMakeDebug[arm64-v8a] :app:buildCMakeDebug[arm64-v8a]-2`
- `android\\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.CdAudioSourceVisibilityTest"`
- `android\\run-code-quality.ps1 -Fix -Paths ...` still falls back to a repo-wide pass in this environment and reports pre-existing `SetupActivity.kt` max-line-length debt outside this change set
