# KCXF2 Chromaprint Names and Background Progress

## Goal

- Make bundled Chromaprint matches use curated mission track-list titles instead of generated archive/HOG names.
- Generate missing mission music Chromaprints as part of launcher background analysis.
- Start music hashing only after level metadata work for the same mission has reached a terminal state.
- Show music hashing in a second Advanced tab progress bar and include its events in the existing route metadata precompute log.

## Investigation Result

- [x] Confirmed `game_data/mission_files/KCXF2RMv11.tracklist.json` contains the eight intended titles.
- [x] Confirmed `game_data/music/Mission ZIP - KCXF2RMv11/chromaprint_info.jsonc` preserves those titles as `tracklist_name` with `name_source: tracklist`.
- [x] Confirmed `android/app/src/main/assets/known_albums.jsonc` also preserves `tracklist_name`, while its `name` field intentionally remains the mechanical source basename such as `KCXF2RMv11.7z_KCXF2RM.hog_game01`.
- [x] Found the runtime loss in `flattenFingerprintDatabase()` in `FingerprintBridge.kt`. It projects `acoustid_name` or `name` into the native matcher and never reads `tracklist_name`.
- [x] Traced the visible regression to the AcoustID validation work in commit `5967421a`. Before that change, curated titles were duplicated through `acoustid_name`. The hardening correctly stopped treating track-list data as reviewed AcoustID data, but the runtime projection had no explicit track-list branch and fell back to the mechanical `name`.
- [x] Confirmed the existing unit projection test checks album inclusion but not title-source precedence, so this case was not covered.
- [x] Confirmed cached mission fingerprint rows store both the selected local name and the fingerprint database identity. `identifyLocal()` can rematch a cached fingerprint after the bundled database changes, but the current dialog-level pending filter skips every cached track without checking that identity. Background work must treat a stale database identity as pending so corrected titles are republished without decoding the audio again.

## Proposed Name Precedence Fix

- [x] Add one shared title-selection helper used by both fingerprint database projection paths in `FingerprintBridge.kt`.
- [x] Give an explicit, nonblank `tracklist_name` first priority when `name_source` is `tracklist`.
- [x] Keep reviewed/agreeing `acoustid_name` next, followed by the maintained `name`, then the numbered fallback.
- [x] Preserve the AcoustID agreement policy for external results. Do not route curated track-list titles through that policy because their authority is the checked-in track list, not filename similarity.
- [x] Extend `DiscDatabaseContractTest` with KCXF2-shaped data proving the flattened native entry is `Birth of the Manul (ft. Ocelot Spirit)` rather than the mechanical basename.
- [x] Add a focused asset contract test that parses the checked-in KCXF2 album records and verifies all eight projected runtime names against `KCXF2RMv11.tracklist.json`.

## Background Hashing Design

- [x] Introduce a mission-music background job model keyed by the same installed archive identity used by the route mission jobs.
- [x] During coordinator discovery, inspect installed mission ZIP/7z mods with `MissionZipMusic.inspect()`, retain only supported compressed-audio tracks, and group them by mission archive.
- [x] Define an eligible music job as one whose route jobs for the same mission are all terminal (`complete` or recorded `failed`). A failed level must not block hashing forever, but the log and status snapshot should retain the level failure count.
- [x] Prefer finishing the eligible mission's music job before moving to unrelated fill-priority route work. Newly imported and recent-level route work must retain the current higher-priority/preemption behavior.
- [x] Reuse `MissionZipMusicStageManager` and `MissionZipAudioFingerprintCache.identifyLocal()` so archive, nested DXA/HOG, extraction-budget, atomic-cache, and local-match behavior stay single-source.
- [x] Treat a track as complete only when its source identity is current, its Chromaprint is nonblank, and its `localMatchDbIdentity` equals `FingerprintBridge.databaseIdentity()`. A stale database match reuses the cached Chromaprint and only reruns bundled matching.
- [x] Keep optional AcoustID web lookup out of background hashing. This phase is local Chromaprint generation plus bundled-database matching only.
- [x] Check cancellation between tracks, clean staged files with the existing stage manager policy, and share the coordinator's game-launch, power-save, thermal, metadata-viewer, and content-import lifecycle.
- [x] Publish the engine music-name sidecar after each successful track so decoded names are available without opening the music dialog.

## Advanced Tab Visibility

- [x] Extend `RouteMetadataPrecomputeSnapshot` with a separate music section: total tracks, finished tracks, failed tracks, current mission, current track, phase, and update time.
- [x] Add `MUSIC` discovery, start, track-finished, mission-finished, and failure lines to `route_metadata_precompute.log`; keep the existing Open Full Log and Export actions unchanged.
- [x] Preserve the existing route metadata progress bar as the first bar.
- [x] Add a second `Chromaprint Hashing` determinate bar using finished/total tracks, with current mission/track detail and stale-worker warning behavior matching the route section.
- [x] Show stable empty, waiting-for-level-metadata, active, complete, and complete-with-failures labels. Do not merge route and music counts into one percentage.
- [x] Keep monitor JSON reads backward-safe through default values because the status file is disposable launcher state, not a migration target.

## Verification

- [x] Add scheduler tests proving a mission music job is ineligible while any same-mission route level is nonterminal and becomes eligible after the last route attempt is terminal.
- [x] Add ordering tests proving eligible music for a completed mission does not preempt newly imported/recent route work, but runs before unrelated fill work.
- [x] Add pending-work tests proving current hashes are skipped, blank hashes are retried, and stale database identities are rematched.
- [x] Extend monitor tests for independent route/music counters and shared `MUSIC` log events.
- [x] Add an Advanced progress model/UI helper test for waiting, active, complete, and failed states.
- [x] Run the focused JVM test classes, then the Android unit-test task.
- [x] Run `android/tests/test_jsonc_and_tracklist_parsing.ps1` and the fingerprint asset/strict-match checks.
- [x] Run scoped code quality over every changed Kotlin, PowerShell, and plan/test path.
- [x] Build the debug APK with JDK 21.
- [ ] Import or provision KCXF2 on the emulator, wait for its level metadata to finish, verify the second bar hashes all eight tracks afterward, and confirm the UI/sidecar reports the curated titles.
- [ ] Export the shared precompute log and verify route completion precedes KCXF2 `MUSIC` start entries.
