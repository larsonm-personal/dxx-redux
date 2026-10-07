# Gameplay audio normalization

Finish the user's normalization request with fresh-install effects and music
sliders at 8, letting the device master volume control listening level

- [x] Separate effects calibration from its slider default, preserving the
      corrected 2/8 effects level initially with a fixed 0.25 gain at the new 8/8
- [x] Preserve the measured relative MIDI/CD/file music calibration
- [x] Capture the actual combined music/effects output and isolated music for
      both games, including MIDI, CD and MP3 playback and representative effects
- [x] Measure loudness and peaks, tune if evidence warrants, and retain a
      repeatable integration runner with audible capture artifacts
- [x] Verify fresh defaults, attenuation/mute behavior, both Android builds,
      regression checks, scoped formatting and automation catalogs

Do not alter unrelated work or outstanding_bugs.md. Retain the Android mixer
double-attenuation fix and preserve desktop behavior. Do not introduce dynamic
compression or claim arbitrary MP3 recordings have been individually normalized

Completion requires actual runtime captures and a report of their music/effects
balance and combined peaks. Source-only music measurements or moving a slider
default are insufficient. Finite fixtures establish tested headroom, not a
guarantee against clipping for every mod, soundfont or simultaneous effect

## Calibration decision

The fresh-install report was explained by effects receiving the slider twice:
once as channel volume and again as startup distance attenuation. At the former
2/8 default that made a full-level effect about 18 dB quieter than intended.
Keep the corrected distance handling and move the intended effects calibration
into a fixed 0.25 multiplier, with both sliders defaulting to 8/8.

Do not retune MIDI against that effects bug. Preserve the source gains: MIDI
-7 dB, D2 bundled-SF2 adjustment 0 dB, CD and decoded file music -14 dB. The
19-selection host comparison measured a 0.175 dB median D2/CD gap and no MIDI
boundary samples. These are source levels before the shared gameplay trim.

Overlap testing showed that source-only headroom is insufficient. A long D2
game02 capture with eight simultaneous explosions reached +0.07 dBTP and nine
boundary samples even after lowering the effects-only scale to 0.1875. The
final design instead preserves the intended effects/music balance: effects
calibration 0.25, then a common 0.625 (-4.08 dB) trim on effects, MIDI, CD and
file playback before summation. Net effects scale is 0.15625. This keeps
compression out of the signal path. Launcher previews retain separate levels.

The integration fixture asserts unpaused foreground gameplay, includes the
full high-peaking game02 passage, and reports normal first-ten-second output
separately from explosion stress. Matched CD/MP3 fixtures use the same track-5
excerpt; their measured music levels must differ by less than 1 dB.

## Final device measurements

Built D1 and D2 in the Android x86_64 debug APK, installed on emulator-5582,
and completed `test_audio_mix.ps1` with all seven cases passing. Every case
confirmed fresh 8/8 defaults, effects channel gains 20/10/0 at slider 8/4/0,
foreground unpaused gameplay and exact silence when both sliders reached zero.
All music and final-mix captures had zero PCM boundary samples.

| Case         | Music LUFS | Combined LUFS | Combined true peak dBFS |
| ------------ | ---------: | ------------: | ----------------------: |
| d2-midi-peak |     -25.74 |        -18.30 |                   -1.50 |
| d2-midi      |     -44.29 |        -19.49 |                   -3.78 |
| d2-cd        |     -28.70 |        -21.24 |                   -3.68 |
| d2-mp3       |     -28.69 |        -21.27 |                   -3.49 |
| d1-midi      |     -33.06 |        -21.57 |                   -2.93 |
| d1-cd        |     -28.72 |        -21.10 |                   -3.06 |
| d1-mp3       |     -28.68 |        -21.07 |                   -3.04 |

Matched-source CD/MP3 music gaps were 0.01 dB in D2 and 0.04 dB in D1.
The loud MIDI case lasted 130 seconds; its music peak was -6.91 dBFS and
combined peak -1.50 dBFS. Different arrangements and quiet introductions
remain different in loudness; this is source-family calibration, not automatic
per-song leveling. Integrated full-capture levels include deliberate explosion
stress; the report separately records normal first-ten-second measurements.

Artifacts: `temp/audio-mix-normalization/report.html`, adjacent JSON and raw
WAVs; source-only comparison `temp/audio-normalization-music/report.json`;
build log `temp/audio-normalization-build-final.txt`. The emulator verifies
digital output, not maximum loudness through a particular phone's speakers.

Validation completed: Android build succeeded with no new compiler warnings;
`test_sound_trace.ps1` passed the full-level/quieter-sound distance and calibrated
channel-gain checks; scoped mixed-language code quality passed. Automation
catalog validation passed (94 standalone JSON tests, 372 support scripts,
197 standalone PowerShell tests), as did master catalog integration (295 entries).
The new runner is registered with a 900-second master timeout. Desktop gain
paths and launcher preview levels are unchanged.
