# Obsidian SNG Track List View Plan

Status: implemented and verified 2026-08-11

## Goal

- Make a top-level `.sng` row in mission ZIP metadata open a track view scoped to that song-list file.
- Reuse the existing mission music browser, staging, preview, fingerprint, and name-display behavior.
- Implement the SNG view as a mildly customized mode of `MissionZipMusicDialog`, not as a second track viewer.
- Preserve the song list's order and repeated rows.
- Do not merge or hide MIDI and rendered-audio variants that happen to have the same logical song name.

## Investigation checklist

- [x] Trace metadata constituent taps and the existing mission music dialog.
- [x] Trace `.sng` parsing, source attribution, and archive/extracted-file handling.
- [x] Inspect Obsidian's recovered original song list and HOG contents.
- [x] Compare the launcher parser with the D1 and D2 engine song-slot semantics.
- [x] Identify focused unit and integration coverage.
- [x] Record the recommended design, implementation phases, and exit gates.

## Findings

### Current metadata flow

- `ModDetailsDialog` in `SetupSections.kt` renders every mission ZIP constituent as an `OutlinedButton`. Every file currently sets `constituentTarget`, which opens the generic `MissionZipConstituentDialog`.
- `missionZipViewAction` only adds a secondary action for inline or external documentation. A `.sng` therefore has no track-list action and its generic file dialog has little useful content beyond type, role, path, and size.
- The same mod-details dialog already has a top-level `Music tracks` button. It opens `MissionZipMusicDialog` with the complete `MissionZipMusicCatalog`.
- `MissionZipMusicDialog` already owns the useful behavior that should be reused: MIDI and compressed-audio preview, cache staging, passive local fingerprint matching, optional AcoustID lookup, decoded names, progress, and storage errors.

### Current music model limitation

- `MissionZipMusic.inspect` and `inspectExtracted` scan outer archives, nested DXAs, and HOGs into sources. Each source merges all `.sng` references and playable files into one display list.
- `SourceBuilder` remembers the outer `archiveEntryPath` for an unresolved reference, but a reference that resolves to a playable file becomes a copy of that playable track and loses the identity of the `.sng` that listed it.
- Multiple `.sng` files in one source are merged, and `buildDisplayTracks` intentionally removes repeated reference names for the all-music browser. Filtering the existing display tracks by `archiveEntryPath` would therefore be incorrect for a faithful per-file view.
- A scoped view must retain one ordered record per top-level `.sng`, including duplicate rows, and resolve those records against playable tracks separately from the existing all-music presentation.

### Obsidian behavior

- The original archive is no longer at `game_data/Obsidian.zip`, but a recovered extraction is available under `android/temp/mission_zip_host_metadata/20260804_230240/raw/obsidian`.
- `obsidian.sng` contains 14 rows in this exact order:
  1. `descent.hmp`
  2. `briefing.hmp`
  3. `endlevel.hmp`
  4. `endgame.hmp`
  5. `credits.hmp`
  6. `game01.hmp` through 14. `game09.hmp`
- `obsidian.hog` contains `Credits` and `game01` through `game09` in HMP, HMQ, and MID forms. The SNG explicitly selects the HMP forms. The first four SNG names are not in the mod and are expected to resolve from mounted base data at runtime.
- The launcher publishes Obsidian's single custom-named top-level `obsidian.sng` as the generated root `descent.sng`. The engine first tries `dxx-r.sng`, then `descent.sng`.
- D1 and D2 use the same slot indices from `main/songs.h`: title, briefing, end level, end game, credits, then level music. Level tracks cycle when a mission has more level numbers than SNG level rows. Obsidian has 15 normal and 3 secret levels but only 9 level-music rows, so cycling is expected.
- The current full `Music tracks` view correctly exposes the SNG references and the separately contained HMP, HMQ, and MID files. Those apparent duplicate song names should remain unchanged.

## Recommended design

### Represent top-level song lists explicitly

Add a small song-list model to `MissionZipMusic.kt`, for example:

```kotlin
data class MissionZipSongList(
    val archiveEntryPath: String,
    val displayName: String,
    val references: List<String>,
)
```

Add `songLists: List<MissionZipSongList> = emptyList()` to `MissionZipMusicCatalog`. Keeping a default avoids noisy changes to tests and helper code that construct catalogs directly.

During outer-archive and extracted-record scans:

- Parse each top-level `.sng` once with the existing bounded descriptor read.
- Preserve its normalized original archive-entry path.
- Preserve reference order and duplicate rows in `MissionZipSongList.references`.
- Continue feeding the same parsed references to `SourceBuilder` so the existing full `Music tracks` view and its ordering do not change.

Do not add nested DXA or HOG song lists to this first change. The metadata screen only offers direct rows for outer constituents, so nested lists have no tap target. Their existing contribution to the complete catalog remains unchanged. The model can be extended with nested locators later if the HOG/DXA contents explorer gains row actions.

### Resolve a scoped list without mutating the full catalog

Add a pure helper that maps a `MissionZipSongList` to ordered display entries using playable tracks from the complete catalog:

- Match the normalized full reference name case-insensitively first.
- If no full-name match exists, use a leaf-name match only when exactly one playable candidate exists.
- Keep an ambiguous reference unresolved instead of choosing based on archive order.
- Keep every SNG row, even when the same filename appears more than once.
- Reuse the matched track object and ID so staging, MIDI preview, fingerprint cache, and decoded names continue to work.
- Describe an unresolved row as `Referenced, not included in this mod`, not `Missing`. Obsidian demonstrates that base game data can validly supply such a file.

This resolution is exact-name based. `game01.hmp`, `game01.mid`, and `game01.ogg` remain distinct; there is no stem grouping, preferred-format logic, or duplicate suppression.

### Parameterize the existing dialog

Keep the original catalog as the operational input to `MissionZipMusicDialog`, and add an optional selected song list or view specification. Do not manufacture a reduced catalog, because the staging manager needs the original archive identity and complete source locators.

Maximum reuse is an implementation constraint:

- Keep one `MissionZipMusicDialog` composable and one track-row rendering path.
- Derive `visibleSources` or equivalent presentation rows from the optional selected SNG before entering the shared rendering path.
- Express SNG differences as data: dialog title, header text, source heading visibility, slot-purpose subtitle, and the visible track collection.
- Do not duplicate preview buttons, staging coroutines, fingerprint controls, progress reporting, decoded-name rendering, or storage-error handling.
- If a customization cannot be represented cleanly as presentation data, prefer the existing all-track behavior over adding a parallel SNG-only control path.

For the complete view:

- Preserve the current `Music tracks` title, grouped sources, counts, duplicate variants, lookup controls, and behavior.

For a selected `.sng`:

- Use a title such as `obsidian.sng tracks`.
- Show the SNG path and `14 entries` rather than the mod archive's private absolute path.
- Render only that SNG's ordered entries, without archive/HOG source group headings.
- Show a compact purpose label derived from the engine slots: `Title`, `Briefing`, `End level`, `End game`, `Credits`, then `Level music 1`, `Level music 2`, and so on.
- Keep the referenced filename as the primary row text. The purpose is supporting context, not a replacement title.
- Offer `Preview` only when the exact reference resolves to a playable catalog track.
- Retain decoded names and fingerprint details for resolved compressed-audio rows.
- Limit passive analysis and `Lookup all` to resolved compressed-audio rows visible in the selected SNG, rather than analyzing unrelated files elsewhere in the mod.

Document the five shared slot indices beside the Kotlin labels because `d1/main/songs.h` and `d2/main/songs.h` remain the runtime source of truth.

### Route the metadata tap directly

In `ModDetailsDialog`, replace the catalog-only dialog target with a target that can represent either the full catalog or one selected song list.

- When a constituent path matches a catalog song list, tapping that file row opens the scoped music dialog directly.
- Change that row's secondary text from `Other file` to `Song list - 14 entries - 609 B` using the catalog count.
- Other constituent rows continue opening `MissionZipConstituentDialog`.
- If music scanning failed or the SNG is empty, retain the current generic constituent-details fallback instead of creating a broken action.
- Keep the separate top-level `Music tracks` button as the complete, unfiltered browser.

The scoped header retains the useful path, count, and inclusion status, so bypassing the generic SNG details dialog does not discard meaningful information.

## Implementation phases

### Phase 1: Preserve song-list identity

- [x] Add the top-level `MissionZipSongList` collection to `MissionZipMusicCatalog`.
- [x] Refactor song-list parsing so a scan can reuse one parsed ordered list for both the new model and the existing `SourceBuilder` input.
- [x] Populate it for ordinary archives, 7z mission archives, and durable extracted mission ZIP records.
- [x] Keep full-catalog source construction and source identity behavior stable.

Exit gate: a catalog can retrieve `obsidian.sng` by normalized constituent path and returns all 14 references in file order.

### Phase 2: Add scoped resolution and presentation data

- [x] Add the pure exact-path/unique-leaf resolver.
- [x] Preserve repeated SNG rows and represent unresolved or ambiguous rows explicitly.
- [x] Add the engine-slot purpose labels with a source-of-truth comment pointing to both `songs.h` copies.
- [x] Derive visible fingerprint/lookup work from scoped entries.

Exit gate: an Obsidian-style fixture resolves `credits.hmp` and `game01.hmp` through the HOG, leaves the four base references non-previewable, and does not select HMQ or MID siblings.

### Phase 3: Reuse and scope the music dialog

- [x] Parameterize `MissionZipMusicDialog` for all-tracks and selected-SNG modes.
- [x] Share the existing preview, staging, cached-name, lookup, progress, and error code paths.
- [x] Apply the scoped title, path/count summary, purpose labels, and `not included in this mod` text.
- [x] Confirm the complete view is visually and behaviorally unchanged.

Exit gate: the selected-SNG dialog shows only ordered SNG entries and preview buttons work through the original catalog.

### Phase 4: Wire metadata navigation and row copy

- [x] Match each top-level `.sng` constituent to its catalog song-list record by normalized case-insensitive path.
- [x] Route a matched row directly to the selected-SNG dialog.
- [x] Show `Song list`, the entry count, and file size on the metadata row.
- [x] Preserve generic-details fallback for unreadable or empty lists.

Exit gate: tapping `obsidian.sng` opens its 14-entry scoped view, while tapping `obsidian.hog` still opens constituent metadata and tapping `Music tracks` still opens the complete catalog.

### Phase 5: Regression and device verification

- [x] Extend `MissionZipMusicTest` for independent top-level SNGs, repeated rows, cross-HOG resolution, and mixed HMP/HMQ/MID siblings.
- [x] Extend `MissionZipMusicExtractedPreviewTest` so an extracted custom-named SNG retains its original constituent identity even though launch staging also generates `descent.sng`.
- [x] Add focused unit coverage for purpose labels used by `SetupSections.kt`.
- [x] Run the focused Gradle tests for `MissionZipMusicTest`, `MissionZipMusicExtractedPreviewTest`, `MissionZipMusicStageManagerTest`, and `MissionZipMusicDisplayTest`.
- [x] Run scoped code quality on the changed Kotlin, tests, and this plan, followed by `git diff --check`.
- [x] On the emulator, open the real imported Obsidian metadata and verify the 14-row scoped list, HMP previews for contained tracks, non-missing wording for the four base references, close behavior, scrolling through `game09.hmp`, and the unchanged 44-entry full `Music tracks` view.

Exit gate: focused tests and scoped lint pass, and the Obsidian metadata flow is verified end to end on Android.

## Implementation notes

- `MissionZipMusicDialog` remains the only mission music viewer. An optional `MissionZipMusicSongList` selects scoped presentation rows before the existing shared rendering loop.
- The scoped mode reuses the original catalog for preview staging and fingerprint identity. It does not create a filtered catalog or duplicate any preview controls.
- The real extracted Obsidian catalog resolves `credits.hmp` and `game01.hmp` through `game09.hmp` from `missions/obsidian.hog`; `descent.hmp`, `briefing.hmp`, `endlevel.hmp`, and `endgame.hmp` remain valid non-previewable base-data references.

Verification:

- `android\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.MissionZipMusicTest --tests com.dxxredux.app.MissionZipMusicExtractedPreviewTest --tests com.dxxredux.app.MissionZipMusicStageManagerTest --tests com.dxxredux.app.MissionZipMusicDisplayTest`
- `android\gradlew.bat :app:assembleDebug`
- Scoped `android\\run-code-quality.ps1 -Fix` for the changed Kotlin, tests, and plan
- Installed the debug APK with preserved app data and inspected the real Obsidian Compose UI hierarchy on `emulator-5554`

## Non-goals

- Do not deduplicate or group MIDI and MP3/OGG/WAV versions by filename stem.
- Do not change which SNG the launcher publishes or which SNG the engine prefers.
- Do not resolve references against installed base game data for launcher preview in this change.
- Do not make nested HOG/DXA metadata content rows interactive in this change.
- Do not change the full `Music tracks` catalog semantics or ordering.
