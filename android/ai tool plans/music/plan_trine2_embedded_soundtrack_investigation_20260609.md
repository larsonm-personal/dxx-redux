# Trine2 Embedded Soundtrack Investigation - 2026-06-09

## Goal
Inspect `game_data/mission_files/trine2.zip` for the claimed custom OGG soundtrack, determine where the audio is stored, and update the ZIP music metadata/browser plan so preview, chromaprint, and in-game playback cover this layout.

## Plan
- [x] Inspect outer ZIP entries, file sizes, descriptors, and song lists.
- [x] Inspect nested HOG/DXA archives for OGG/MP3/FLAC/WAV/HMP/MID content.
- [x] Determine how the current launcher/game staging path would expose those tracks to the engine.
- [x] Update the broader ZIP music plan with any needed architecture changes.
- [x] Summarize findings and recommended implementation changes.

## Findings
- `trine2.zip` is about 51.9 MB and has only three outer entries:
  - `trine2.hog`: 53,248,255 bytes uncompressed, 51,912,675 bytes compressed.
  - `trine2.msn`: mission descriptor.
  - `Trine2 music.txt`: soundtrack credits.
- `trine2.msn` says:
  - mission name: `Trine - Episode 2`
  - `num_levels = 9`
  - one secret level, `e2m10.rdl,5`
  - `custom_music = yes`
  - `custom_textures = yes`
- The OGG soundtrack is embedded inside `trine2.hog`, not loose in the ZIP.
- `trine2.hog` contains 68 entries:
  - 14 `.ogg` tracks.
  - 1 `descent.sng`.
  - 10 `.rdl` levels.
  - 10 `.dtx` texture sets.
  - 10 `.txb` text files.
  - 23 `.bbm` images.
- Audio-like payload is 51,260,967 bytes, about 96.3 percent of the HOG payload.

## Trine 2 Song List
`descent.sng` inside `trine2.hog` lists:
- `descent.ogg`
- `briefing.ogg`
- `endlevel.ogg`
- `endgame.ogg`
- `credits.ogg`
- `game01.ogg`
- `game02.ogg`
- `game03.ogg`
- `game04.ogg`
- `game05.ogg`
- `game06.ogg`
- `game07.ogg`
- `game08.ogg`
- `game09.ogg`

`Trine2 music.txt` maps those tracks to credits:
- Title: Verran, `Descent 1 Menu Remix`
- Exit: Vertigo Fox, `Gettin' Out`
- Endgame: Vertigo Fox, `Descent 1 End Game remix`
- Level 1: RoeTaKa and EvilHorde, `Infiltrator`
- Level 2: Karl Casey/White Bat Audio, `Invective`
- Level 3: Karl Casey/White Bat Audio, `Get Your Ass to Mars`
- Level 4: Verran, `Inward Journey`
- Level 5: Vertigo Fox, `Myriad (Instrumental Mix)`
- Level 6: Johann Langlie, `Robot Jungle (Extended Remix)`
- Level 7: Aim to Head, `Tower`
- Level 8: Vertigo Fox, `Defier (Instrumental Mix)`
- Level 9: Vertigo Fox, `Hunting Shadows`

## Current In-Game Path
- `ModManager.extractMissionZipForLaunch` stages the whole mission ZIP under `.generated_mission_zips/<zip>/missions`, so `trine2.hog` should be available to the game as a mission HOG.
- `ModManager.hasEnabledMissionZipBuiltinMusic` already detects music inside a HOG by checking HOG entry extensions against `flac`, `hmp`, `mid`, `mp3`, and `ogg`.
- `SetupConfigFiles.writeMusicConfigForLaunch` auto-selects built-in/addon music when:
  - there is no explicit music override,
  - a game is being launched,
  - global music mode is `cd`,
  - an enabled mission ZIP has built-in music.
- `d1/main/songs.c` and `d2/main/songs.c` read `dxx-r.sng` or `descent.sng` through PhysFS, accept `.ogg`, `.flac`, `.mp3`, `.mid`, and `.hmp`, and play built-in/addon music through `songs_play_file`.
- Android's `digi_tsf_music.c` implementation of `mix_play_file` reads `.ogg/.mp3/.flac` through PhysFS first, which is the right path for HOG-contained music.

## Consequences For The ZIP Music Browser Plan
- HOG-contained compressed audio must be first-class support.
- Top-level ZIP scanning is insufficient. The scanner must parse HOG directories inside mission ZIPs and detect both:
  - `descent.sng` / `dxx-r.sng`
  - member tracks such as `game01.ogg`
- Preview and chromaprint should extract only the selected HOG member to a temp file, not the full HOG.
- Cache keys need nested identity fields:
  - outer ZIP identity,
  - HOG entry path,
  - HOG member name,
  - content SHA-256.
- The in-game path is probably already structurally correct for default CD-mode users, but the current auto-selection policy is tied to global `music_mode == "cd"`. If the user selected MIDI or custom files, mission-provided OGG music will not automatically play. This should become an explicit UX decision or preference.
- The broader ZIP music plan now defines that policy as a Game Preferences toggle: `Use mission soundtrack when available`, backed by `use_mission_soundtrack_when_available`, defaulting to true.

## Other Large ZIPs With HOG-Embedded Audio
A quick scan of the largest mission ZIPs found more examples:
- `cererian_1.3.zip` -> `cererian.hog`: about 61.7 MB audio-like payload.
- `Trine1.zip` -> `trine1.hog`: about 58.8 MB audio-like payload.
- `U3AAH.zip` -> `U3AAH.hog`: about 47.8 MB audio-like payload.
- `trine2.zip` -> `trine2.hog`: about 48.9 MB audio-like payload.
