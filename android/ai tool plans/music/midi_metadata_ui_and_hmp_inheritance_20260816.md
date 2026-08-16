# MIDI Metadata UI and HMP Inheritance Plan

Status: planned 2026-08-16

## Goal

- Expose a basic, bounded printout of embedded MIDI metadata from the launcher MIDI player
- Expose the same metadata from both the complete mission music browser and a selected SNG track list
- Derive a concise title and composer label for the in-game music list when no curated or fingerprint name exists
- Let an HMP inherit metadata from an exact same-name MID sibling, such as `game05.hmp` and `game05.mid`
- Mark inherited launcher metadata with `Inherited from MIDI version`
- Keep one parser and one title/composer policy shared by all three surfaces

## Investigation completed

- [x] Read the attached Obsidian analysis and its recovered title and composer examples
- [x] Trace installed HOG MIDI enumeration and preview through `MidiEnumerationBridge`, `midi_enumeration.c`, `MusicPickerPage.kt`, and `MidiBytesPreviewDialog.kt`
- [x] Trace mission ZIP, DXA, HOG, SNG, and extracted-bundle tracks through `MissionZipMusic.kt`, `MissionZipMusicStageManager.kt`, and `MissionZipMusicDialog`
- [x] Trace the in-game list through `songs_get_track_list`, `MusicControlPanel`, `track_names.c`, and `mission_music_names.json`
- [x] Inspect the recovered Obsidian HOG and confirm that it contains `game01` through `game09` in HMP, HMQ, and MID forms
- [x] Confirm that the shared HMP-to-MIDI converter preserves ordinary MIDI meta events when they exist
- [x] Identify focused native, JVM, launcher-introspection, and in-game verification points

## Findings

### The metadata is in standard MIDI meta events

The supplied Obsidian analysis matches the recovered HOG contents. The MID files contain sequence names, track names, copyright records, free text, dates, contact information, and composer lines. The corresponding HMP files have no useful text in this pack.

Useful Obsidian examples include:

- `game01.mid`: generic sequence name `untitled`, followed by metadata-only text that identifies `Doug Hale` and `Hotshot`
- `game03.mid`: `Boss` and `By Doug Hale`
- `game04.mid`: `untitled` and `Copyright 1998 by Verran Eventide`
- `game05.mid`: `Created for Final Insertion Levels; Descent 2`, a copyright line, and Verran contact text
- `game06.mid`: generic sequence name followed by metadata-only text that identifies `Doug Hale` and `The Subtegon`
- `game07.mid`: `Untitled, pt. 2 - Connected` and `By Doug Hale`
- `game09.mid`: generic sequence name followed by metadata-only text that identifies `Doug Hale` and `Untitled, pt. 3 - Detached`

There is no standard MIDI composer field. Composer and fallback title selection therefore require a conservative documented heuristic. Raw events must remain visible so the user can distinguish embedded facts from inferred summary fields.

### The launcher currently has two independent MIDI catalogs

- The general MIDI picker enumerates installed base and mission HOGs in native code. Its `TrackInfo` contains only filename and duration. Opening a row immediately opens `MidiBytesPreviewDialog`.
- The mission metadata browser catalogs ZIP, 7z, extracted bundle, nested DXA, and HOG tracks in Kotlin. It can read bounded MIDI bytes on demand, but it does not parse metadata.

Both paths already converge on `MidiBytesPreviewDialog` for playback. A shared metadata dialog and a native byte-parser bridge can serve both without creating another player.

### The in-game list already has the correct name precedence hook

`songs_get_track_list` and `songs_get_track_info` call `mission_music_names_lookup` before falling back to the raw `BIMSongs` filename. `track_overlay_notify_mission_music` uses the same lookup for the transient track overlay.

The best integration point is to extend this resolver so its precedence is:

1. Existing curated or fingerprint-derived mission sidecar name
2. Direct useful metadata embedded in the selected MIDI or HMP
3. Metadata inherited by an HMP from its exact MID sibling
4. Existing raw filename fallback

This avoids changing the in-game JSON contract or `MusicControlPanel`. It also covers mounted loose missions and HOGs, not only mission ZIPs that happened to receive a launcher sidecar.

### HMP inheritance needs container scope

A stem-only global match could incorrectly borrow metadata from another HOG or nested archive. Inheritance must require all of the following:

- The displayed file has extension `.hmp`, case-insensitively
- The candidate has extension `.mid`, case-insensitively
- Replacing only the HMP extension with MID produces the same normalized relative name
- Both entries belong to the same logical source and container
- Exactly one MID candidate matches
- Direct useful HMP metadata wins; inheritance is used only when the HMP lacks a useful title, composer, or text printout

For the native game resolver, the current PhysFS search path is the logical container scope and the peer is the exact extension-replaced path. For mission catalog objects, source ID plus archive, nested entry, HOG entry, and relative parent path define the scope. Do not match across sources, and do not inherit based only on a leaf name when directories differ.

Limit this first implementation to `.hmp` to `.mid`, as requested. HMQ continues to show any direct metadata it contains but does not inherit from MID.

## Recommended design

### 1. Add one bounded native metadata parser

Add `shared/midi_metadata.c` and `shared/midi_metadata.h` and compile them into both Android game libraries and a host test target.

The parser should:

- Accept raw SMF MIDI or HMP bytes plus the known input kind
- Convert HMP through the existing `hmp2mid_mem` path, which already preserves meta events
- Validate `MThd`, header length, format, declared track count, `MTrk` chunk bounds, variable-length quantities, running status, SysEx lengths, meta-event lengths, and complete event payloads
- Collect text meta types 0x01 through 0x09 with track index and a stable user-facing type label
- Record header format, track count, and time division for the basic printout
- Record whether each track contains channel events so title inference can distinguish metadata-only tracks from instrument tracks
- Decode valid UTF-8 as UTF-8, otherwise decode legacy bytes as Windows-1252, and always emit valid null-free UTF-8
- Normalize embedded CR/LF and surrounding whitespace without merging distinct events
- Bound input using the existing 64 MiB MIDI read limit
- Bound retained output independently, for example 256 text events, 4 KiB per event, and 64 KiB total text, with explicit truncation flags in the result
- Reject malformed structure without partial out-of-bounds parsing and return a stable error code and empty summary

The result should contain:

```text
format
track_count
time_division
text_events[]: track_index, type, text
title
composer
display_name
metadata_truncated
```

Keep inheritance outside the byte parser. Inheritance relates two catalog entries, not one MIDI file.

### 2. Use a conservative title and composer policy

Implement title, composer, and concise display-name inference in the native parser so every consumer agrees.

Title precedence:

1. First nonblank, non-generic sequence or track name on MIDI track zero
2. If track zero is blank or only `untitled`, use a plausible short text line from a metadata-only track
3. Otherwise retain `Untitled` only when a useful composer exists; do not replace a filename with an unqualified generic title

Reject fallback-title candidates that are separators, dates, email addresses, URLs, copyright or contact instructions, `by` lines, or ordinary instrument-track names. Do not inspect musical notes or filenames to guess a title.

Composer precedence:

1. A standalone case-insensitive `By <name>` text line
2. A copyright line ending in `by <name>`
3. A short copyright owner when the syntax clearly identifies a person or group

Reject generic owners such as `Me`, blank values, email addresses, and contact prose. Preserve the raw event even when it is rejected as an inferred composer.

Build the concise label as `Title (Composer)`, `Title`, or `Composer` only when that result is more identifying than the raw filename. Cap it to the existing in-game overlay budget, preserve valid UTF-8 boundaries, use `...` for truncation, and reserve room for the composer suffix when both fields exist. The canvas width trimming in `MusicControlPanel` remains the final device-specific fit step.

Add native tests using the Obsidian metadata patterns above. The policy should intentionally produce examples such as `Hotshot (Doug Hale)`, `Boss (Doug Hale)`, and `Untitled, pt. 2 - Connected (Doug Hale)`. `Copyright 1999 by Me` must remain visible in the printout but must not become a composer attribution.

### 3. Expose parser results through one Kotlin bridge

Add `MidiMetadataBridge.kt` with serializable result and event models, backed by a JNI function in `jni_midi_preview.c` or a small adjacent JNI source.

- The JNI call accepts bounded bytes and `isHmp`
- It serializes the native result as strict JSON and returns it with `dxx_jni_string_from_utf8`
- Kotlin treats parse failure as a displayable `No readable MIDI metadata` state, not as a playback failure
- The bridge reuses `MidiEnumerationBridge.nativeDataLock` because HMP conversion shares legacy native state
- Metadata parsing must run on `Dispatchers.IO`

Extend native installed-HOG enumeration to include the same metadata summary and raw event list in each `TrackInfo`. It already reads every MIDI/HMP entry to compute duration, so parse the bytes during that read rather than reopening the HOG.

After one HOG is cataloged, resolve exact HMP/MID sibling pairs within that HOG and copy the MID metadata view to an otherwise metadata-empty HMP. Add `metadataSourceFilename` and `inheritedFromMidi` to the JSON and Kotlin model. Do not change playback bytes or duration when metadata is inherited.

### 4. Add one reusable metadata printout dialog

Add a reusable Compose dialog, preferably next to `MidiBytesPreviewDialog.kt`, that shows:

- File name
- Inferred title and composer when present
- `Inherited from MIDI version: game05.mid` when applicable
- SMF format, track count, and time division
- Text events in file and track order as `Track N - Type: text`
- A clear empty or malformed message
- A truncation note when parser output reached a safety limit
- A vertically scrollable body, Close action, TV focus behavior, and the existing text sizes and color conventions

Add an optional `Metadata` action to `MidiBytesPreviewDialog`. The installed-HOG player can pass the enumeration result directly. Mission ZIP previews can pass a suspend metadata loader that uses the same native bridge and the exact-pair resolver.

Do not mix the raw metadata dump into the playback detail lines. Keeping it behind the button avoids turning long comments and copyright blocks into an unwieldy player.

### 5. Apply it to the installed MIDI picker

Update `MusicPickerPage.kt` and `MidiEnumerationBridge.kt` so:

- Opening a track still opens the existing player
- The player contains a `Metadata` button whenever parsing succeeded or produced a useful empty-state report
- The picker row may show the concise metadata display name as secondary text while keeping the filename as the stable primary identity
- An inherited HMP shows `Inherited from MIDI version` in its secondary text and metadata dialog
- The matched MID remains its own independent playable row
- Selection, duration display, D-pad navigation, and playback behavior remain unchanged

### 6. Apply it to both mission metadata track views

Add a pure helper in `MissionZipMusic.kt` that finds an exact MID peer for an HMP using the container rules above. Return a small resolution object containing the displayed track, metadata source track, and inheritance flag.

In `MissionZipMusicDialog`:

- Add `Metadata` next to `Preview` for playable MIDI/HMP rows in the complete catalog view
- Add the same action in a selected SNG view
- Read and parse bytes on IO through `MissionZipMusicStageManager.readMidiTrackBytes`
- Parse direct HMP metadata first, then parse the exact MID peer only if direct metadata is not useful
- Cache results by catalog source identity and track ID for the dialog lifetime
- Keep SNG resolution exact, so selecting `game05.hmp` does not switch playback to `game05.mid`
- Show `Inherited from MIDI version: game05.mid` in the metadata dialog and compact row subtitle after the result is cached
- Preserve existing compressed-audio fingerprint, AcoustID, staging, progress, and preview paths

Do not eagerly materialize every MIDI in a large archive merely to open the music list. Resolve metadata when its button is used, then retain it for that dialog session. Installed-HOG enumeration remains eager because it already reads all track bytes for duration.

### 7. Feed concise metadata names into the in-game list and overlay

Extend `track_names.c` with a bounded embedded-MIDI cache populated from the current `BIMSongs` after `songs_init` has selected the active SNG and mounted sources.

The existing `mission_music_names_load()` call in both D1 and D2 is already at the correct lifecycle point. Expand the shared implementation to:

- Clear old embedded metadata names on every song reinitialization
- Keep existing `mission_music_names.json` names as first priority
- For each MIDI or HMP `BIMSongs` entry without a sidecar name, read the exact PhysFS file with the existing MIDI size ceiling and parse it once
- If an HMP has no useful direct result, replace only `.hmp` with `.mid`, require that exact PhysFS path to exist, parse it, and cache the resulting display name against the HMP filename
- Leave HMQ and compressed-audio entries unchanged
- Cache only the concise display name needed by the game, not the full raw event printout

Change `mission_music_names_lookup()` to return sidecar names first and cached embedded names second. Existing consumers then gain the result without a new JNI contract:

- `songs_get_track_list` for the in-game picker
- `songs_get_track_info` for current-track state
- `track_overlay_notify_mission_music` for the transient in-level label
- Introspection music track names

Keep this implementation in the Android shared source. No new parsing logic should be added separately to `d1/main/songs.c` and `d2/main/songs.c`, and non-Android desktop behavior must remain unchanged.

## Implementation phases

### Phase 1: Native parser and inference contract

- [ ] Add `midi_metadata.c` and `midi_metadata.h`
- [ ] Parse bounded SMF structure and text meta events
- [ ] Reuse HMP conversion and preserve valid UTF-8 output
- [ ] Implement title, composer, display-name, and truncation policy
- [ ] Add strict native fixtures for malformed input, output limits, encodings, and Obsidian-style metadata

Exit gate: native tests recover the expected Obsidian-style summaries, reject malformed chunks safely, and pass with warnings enabled

### Phase 2: JNI and installed-HOG catalog

- [ ] Add the Kotlin metadata models and native parse bridge
- [ ] Add metadata to installed-HOG enumeration without a second HOG read
- [ ] Add exact same-HOG HMP-to-MID inheritance
- [ ] Extend setup introspection with summary and inheritance fields for automation
- [ ] Add model and JSON decode tests

Exit gate: an installed Obsidian source reports direct MID titles and corresponding inherited HMP summaries while durations and playback remain unchanged

### Phase 3: Shared launcher metadata dialog

- [ ] Add the scrollable metadata printout dialog
- [ ] Add `Metadata` to `MidiBytesPreviewDialog`
- [ ] Show concise summary and inheritance note in the installed MIDI picker
- [ ] Verify touch and TV/D-pad focus behavior

Exit gate: opening `game05.hmp` in the general MIDI picker can play the HMP and separately display metadata sourced from `game05.mid` with the required note

### Phase 4: Mission metadata browser integration

- [ ] Add pure same-container peer resolution to `MissionZipMusic.kt`
- [ ] Add lazy per-session parsing and caching to `MissionZipMusicDialog`
- [ ] Add `Metadata` in complete-catalog and selected-SNG modes
- [ ] Preserve exact HMP playback while displaying inherited MID metadata
- [ ] Add same-source, cross-source rejection, ambiguity, case, directory, and HMQ non-inheritance tests

Exit gate: the full Obsidian music catalog and `obsidian.sng` view both show the HMP inheritance note and the same raw metadata printout as the general picker

### Phase 5: In-game names

- [ ] Populate the shared embedded-MIDI name cache during the existing mission music-name load lifecycle
- [ ] Preserve sidecar-name precedence
- [ ] Add exact PhysFS HMP-to-MID fallback and bounded reads
- [ ] Route list, current track, overlay, and introspection through the expanded resolver
- [ ] Add host tests for cache replacement, name precedence, exact pairing, and fallback filenames

Exit gate: Obsidian's in-game music list shows concise labels such as `Hotshot (Doug Hale)` instead of `game01.hmp`, and switching or restarting missions cannot retain stale names

### Phase 6: Regression and device verification

- [ ] Run the new native metadata and track-name tests through CTest
- [ ] Run focused Gradle tests for MIDI models and mission music resolution/display
- [ ] Extend `test_music_track_controls_unified.json5` only for generic fields that are stable in the checked-in base data
- [ ] Add or run a focused Obsidian launcher and in-game script when the Obsidian archive is available to the test runner
- [ ] Verify direct MID metadata, empty HMP metadata, inherited HMP metadata, malformed MIDI, no peer, ambiguous peer, and stale mission-cache replacement
- [ ] Run a Windows host build, Android debug assembly, scoped code quality, and `git diff --check`

Exit gate: focused tests, host build, Android build, lint, and real-device or emulator verification all pass

## Expected files

Native shared implementation:

- `android/app/src/main/cpp/shared/midi_metadata.c`
- `android/app/src/main/cpp/shared/midi_metadata.h`
- `android/app/src/main/cpp/shared/track_names.c`
- `android/app/src/main/cpp/shared/track_names.h`
- `android/app/src/main/cpp/shared/midi_enumeration.c`
- `android/app/src/main/cpp/jni_midi_preview.c`
- `android/app/src/main/cpp/CMakeLists.txt`
- `android/app/src/main/cpp/extract/CMakeLists.txt`

Launcher and models:

- `android/app/src/main/java/com/dxxredux/app/MidiMetadataBridge.kt`
- `android/app/src/main/java/com/dxxredux/app/MidiEnumerationBridge.kt`
- `android/app/src/main/java/com/dxxredux/app/MidiBytesPreviewDialog.kt`
- `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt`
- `android/app/src/main/java/com/dxxredux/app/MissionZipMusic.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupSections.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupAutomationApi.kt`

Tests:

- New native MIDI metadata and track-name resolver tests under `android/app/src/main/cpp/shared` or `extract`
- New JVM tests for metadata formatting and exact mission peer resolution
- Extensions to `MissionZipMusicTest.kt` and `MissionZipMusicDisplayTest.kt`
- Proportionate updates to `android/game_scripts/test_music_track_controls_unified.json5`

## Risks and controls

- MIDI text is not guaranteed to be UTF-8. Validate UTF-8 first and use one Windows-1252 fallback so JNI and JSON always receive valid Unicode
- Free text can be hostile or extremely large. Keep parser, event, text, JSON, and PhysFS read limits independent
- Composer inference can overclaim. Keep it conservative and always expose raw events beside inferred fields
- Instrument track names can look like song titles. Prefer track-zero sequence names and metadata-only tracks, never arbitrary musical track names
- HMP conversion and other MIDI native paths share legacy state. Continue using the existing native data lock for launcher calls
- Global stem matching can cross-contaminate packs. Require exact extension replacement and same-container identity
- Mission changes can leave stale cached names. Clear and rebuild the embedded cache from the current `BIMSongs` on every song initialization
- Parsing on the render or game loop can stutter. Parse installed catalogs on their existing worker path, parse mission UI metadata on IO after user request, and populate in-game names during song initialization

## Non-goals

- Do not recognize a song by its notes or audio fingerprint
- Do not invent composer attribution from community knowledge that is absent from the file
- Do not add a hand-maintained Obsidian title table
- Do not merge, hide, or redirect playback between HMP, HMQ, and MID rows
- Do not apply HMP inheritance across HOGs, archives, directories, or ambiguous matches
- Do not add HMQ-to-MID inheritance in this tranche
- Do not change MIDI playback, HMP timing conversion, SNG slot ordering, or mission soundtrack selection
