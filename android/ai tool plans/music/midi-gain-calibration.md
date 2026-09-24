# MIDI soundfont gain calibration

Raise shared MIDI soundfont playback by approximately 6 dB relative to FM,
once at the renderer level so gameplay, previews and SF2 fallback agree.

Measure raw SC-55 and FM renders for D1 game01/game07/game08 and D2
descent/game01/game02 (60 seconds each). Check peak headroom before doubling
the soundfont gain, then verify actual output after changing the shared default.
Keep user volume, effects and MIDI event velocities unchanged. Do not normalize
individual songs: FM/GM arrangements and soundfont instrument balances differ.

Run native synth contracts, scoped formatting and an all-ABI Android build.

## Implementation

`shared/music_fluid.cpp` now uses one `default_gain = 0.4` constant for both
initialization and subsequent output/reset configuration, replacing 0.2.
The ratio is +6.0206 dB. The existing user gain remains relative to the same
-10 dB reference, so no preference migration or reset is needed.

## Baseline measurements

Bundled SC-55 1.34, shipping converter/timeline/renderer, stereo 48 kHz,
reverb and chorus enabled. Each sample is the opening 60 seconds.

| Track | MIDI RMS dBFS | FM RMS dBFS | MIDI deficit dB |
| --- | ---: | ---: | ---: |
| D1 game01 | -32.67 | -27.92 | 4.75 |
| D1 game07 | -35.01 | -28.84 | 6.16 |
| D1 game08 | -31.56 | -28.94 | 2.62 |
| D2 descent | -31.63 | -24.73 | 6.91 |
| D2 game01 | -39.81 | -27.42 | 12.38 |
| D2 game02 | -31.23 | -26.92 | 4.32 |

The median deficit is 5.46 dB, supporting the requested fixed +6 dB adjustment.
Arrangement differences explain why one gain does not match every song exactly;
RMS is not a perceptual loudness measurement. No baseline sample would exceed
the signed 16-bit range when doubled.

Reproduction and raw results: `temp/midi-gain-calibration/measure.py`,
`before.json`, `after.json`, and the accompanying WAV files. The script uses
`test_music_synth --render`, which exercises the shared production pipeline.
The baseline was captured before rebuilding that executable with the gain change.

## Validation

- Rebuilt-output checks passed for all six 60-second samples: RMS increased
  by 6.0206 dB (within 0.001 dB), zero PCM boundary samples, highest peak
  -3.17 dBFS (D2 game02). These headroom measurements cover the bundled bank
  and sampled passages, not every possible downloaded soundfont
- Native `music_synth_tests` passed, including reset/seek/loop contracts
- Scoped code formatting/lint passed
- Android `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a and x86_64
