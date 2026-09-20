# Asset alternative fallback survey

- [x] Add movie-library fallback for either resolution and log the selected library
- [x] Check launcher-advertised alternatives and related engine paths: movies, sound banks, demo/full data, briefing text/images, and music formats
- [x] Fix compatible asset-selection mismatches while preserving format-specific decoding and resolution preference when both files exist
- [x] Run scoped formatting, native builds, and focused fallback verification

Do not edit android/outstanding_bugs.md

## Findings and changes

| Asset family | Result |
| --- | --- |
| D2 intro/other/robots/OEM and mission movie libraries | Retry the opposite resolution when mounting the preferred library fails; record the actual mission library for unloading |
| D2 s22/s11 sound banks | Retry the other bank when the selected bank cannot be opened; validate its header before changing sample rate; reinitialize the non-mixer backend when required |
| D2 title, OEM, order and ending PCX pairs | Prefer the requested resolution and select the alternate when only it exists |
| D1/D2 mission briefing and ending tex/txb | Mission loaders already try text and encoded versions; D2 subtitles also retry txb |
| D2 retail/demo HOG, HAM and PIG | Existing engine fallback paths match the launcher's named alternatives; individual required retail level texture banks remain required |
| D1 title PCX | Existing high-resolution preference already falls back to base images |
| Android replacement textures, both games | Existing KTX2 failure path falls through to PNG/JPG/TGA, then the base bitmap |
| Music aliases and WAV, both games | HMQ now follows HMP conversion, MIDI follows MID dispatch, and WAV is admitted by song lists, playback and jukebox scans; Android decodes WAV through SDL into the existing bounded PCM playback path |
| Raw replacement sounds | Sample-rate-specific raw formats retain their rate contract; no blind substitution of differently sampled raw data |

The music extension-admission gaps discovered during the survey were fixed in
this pass. The shared Android WAV adapter reuses SDL's decoder and applies the
existing encoded/decoded PCM size and mono/stereo constraints. Desktop playback
continues through the existing SDL_mixer backend.

## Validation

- `python android/tests/test_asset_alternatives.py` in an MSVC developer shell passed with and without USE_SDLMIXER
- Tests exercise production selection functions: preferred/alternate/missing movies, all movie library categories, mission library unloading, screen pairs, both sound banks/rates, the selected bank's sample payload, disabled audio, invalid headers and both games' song dispatch (including uppercase aliases)
- Windows D1 and D2 CMake builds passed
- Android ARM64 CMake builds for both games passed
- CTest test_music_wav_decode passed with SDL's actual decoder for 8/16-bit mono/stereo sample conversion, invalid input and encoded size rejection
- CTest test_sound_trace_fingerprint and test_args_defaults passed
- Scoped run-code-quality and git diff --check passed; the formatter intentionally excludes upstream D1/D2 source files
- No physical-phone playback verification performed
