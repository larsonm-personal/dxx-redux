# Gameplay audio normalization

Finish the user's normalization request with fresh-install effects and music
sliders at 8, letting the device master volume control listening level

- [ ] Separate effects calibration from its slider default, preserving the
  corrected 2/8 effects level initially with a fixed 0.25 gain at the new 8/8
- [ ] Preserve the measured relative MIDI/CD/file music calibration
- [ ] Capture the actual combined music/effects output and isolated music for
  both games, including MIDI, CD and MP3 playback and representative effects
- [ ] Measure loudness and peaks, tune if evidence warrants, and retain a
  repeatable integration runner with audible capture artifacts
- [ ] Verify fresh defaults, attenuation/mute behavior, both Android builds,
  regression checks, scoped formatting and automation catalogs

Do not alter unrelated work or outstanding_bugs.md. Retain the Android mixer
double-attenuation fix and preserve desktop behavior. Do not introduce dynamic
compression or claim arbitrary MP3 recordings have been individually normalized

Completion requires actual runtime captures and a report of their music/effects
balance and combined peaks. Source-only music measurements or moving a slider
default are insufficient. Finite fixtures establish tested headroom, not a
guarantee against clipping for every mod, soundfont or simultaneous effect
