# Descent Maximum Zophar fingerprints

- [x] Add the supplied archive as a separate music source
- [x] Generate full-track Chromaprints and attempt AcoustID lookups
- [x] Regenerate the bundled Android album database
- [x] Inspect playlist filtering and document movie-track handling

## Source and regeneration

Original: C:/Users/first last/Downloads/Descent Maximum (MP3).zophar.zip
Local pipeline source: game_data/music/Descent Maximum (MP3) - zophar.zip
SHA-256: a88b79f1bf28f0411561a0cc5cf9e87d6cc08bfe6f8807a57c955a67c7d89ba7

Music archives are ignored local inputs, discovered automatically by fingerprint_music_packs.ps1
The checked-in output is game_data/music/Descent Maximum (MP3)/chromaprint_info.jsonc

Run from the repository root:

    ./game_data/fingerprint_music_packs.ps1 -Album 'Descent Maximum (MP3)'
    ./game_data/update_known_discs_albums.ps1 -Force

All 20 files fingerprinted successfully, with no accepted AcoustID labels
The title agreement policy compares filenames; TESTA/TESTB prefixes may prevent external labels from being accepted
Do not weaken that policy or invent recording IDs to populate labels
The merger retains five movie tracks and resolves the other 15 to existing physical-CD fingerprints
The database is android/app/src/main/assets/known_albums.jsonc, loaded by FingerprintBridge

## Movie playback recommendation

Keep identity and playback eligibility separate: retain every fingerprint for recognition
Add curated track roles (movie, title, credits, level, unknown) keyed by source and original filename
Propagate roles through recognition/import and persist them with each custom audio track
Exclude movie tracks from automatic level playlists by default, with a per-track user override
Preserve existing behavior for unknown tracks; do not infer roles from duration or external AcoustID titles
Apply roles before CD deduplication so matching an existing CD track does not erase source-specific roles

The explicit movie entries in this source are (mov) END.mp3, (mov) ESA.mp3,
(mov) INTRO.mp3, (mov) PLA.mp3, and (mov) PLG.mp3
TESTA_1 is labeled Title and TESTA_7 is labeled Crawl (Credits); those should receive their own roles
CustomAudioSetManager now filters filenames starting with (mov), case-insensitively, from level playlists and the active track list
Imported files and fingerprint metadata remain intact; excluded referenced movies are not staged
This hard-coded rule implements the requested interim policy; generalized roles remain a future option

## Validation

- CMake Release fingerprint_audio build passed
- 20 tracks decoded and fingerprinted, zero errors
- Canonical database merger completed: 15 existing CD matches and five new fingerprints
- Fingerprint source identity tests passed
- Gradle :app:mergeDebugAssets passed; merged database SHA-256 equals the checked-in asset
- Source manifest validated: 20 nonempty fingerprints with positive durations
- Scoped code quality and git diff --check passed
