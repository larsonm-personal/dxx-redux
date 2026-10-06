# MIDI / CD loudness calibration

Tuning is centralized in `shared/music_playback_levels.h`, including Android
gameplay synth gain, D2 adjustment, CD attenuation, effects default, and separate
launcher preview gains/multipliers. Each setting has a short scope/units note;
the header summarizes the calibration method. The loudness comparison also
reads the base gain from this header. This refactor preserves the revised levels

Centralization checks passed: both Android engines built, four native CTests,
scoped formatting, and all 19 track measurements/settings/calibration values
matched the previous report exactly (`android/temp/loudness-centralization`)

Current revision: prioritize headroom at the user's request. Remove the +2 dB
D2 MIDI boost, lower CD playback/previews from -12 to -14 dB, and lower the
Android effects default from 3/8 to 2/8 (-12 dB total). Saved effects settings
still take precedence. Keep the calibration hook at zero so integration checks
continue verifying the actual source adjustment

- [x] Apply the revised levels
- [x] Rerun the 19-selection loudness comparison and native tests
- [x] Build both Android engines and verify playback integration

Revised measurements: D2 median -23.67 LUFS, CD median -23.495 LUFS; the gap
remains 0.175 dB. The highest sampled D2 true peak is now -2.82 dBFS, with zero
PCM boundary samples across all 19 selections. Four native CTests passed.
Report: `android/temp/music-headroom-20261005/report.html`

Both Android x86_64 engines built successfully. D1/D2 emulator music integration
passed with zero source boost, as did both automation catalog checks and scoped
code quality

These measurements cover separate music sources, not the final music/effects
mix, and do not guarantee clipping-free playback for every song or soundfont

The following measurements describe the earlier +2 dB revision

- [x] Measure the production bundled SC-55-like synth with default balanced EQ against Definitive Collection D2 CD tracks at the same music slider setting
- [x] Choose a measured MIDI gain adjustment with clipping/headroom checks across representative D1/D2 songs
- [x] Add a repeatable comparison with raw listening samples and readable loudness results
- [x] Validate native builds, synth integration, Android playback, and scoped code quality

Use integrated loudness and true peaks, not peak normalization. D2 MIDI and CD selections are separate arrangements; compare representative playback selections and their distributions without claiming sample alignment. Keep existing uncommitted work and the bug list intact.

The user selected a modest MIDI boost and lower CD playback, preserving dynamics
instead of adding compression or peak limiting

Implemented calibration:

- +2 dB for Android D2 gameplay with the bundled soundfont and balanced EQ
- -12 dB for CD playback and CD previews, using one shared linear scale
- Preserve D1, D1-in-D2, FM, other soundfonts/EQ and MIDI preview synth gains
- Preserve the boost across synth resets, loops and runtime gain commands; clear
  the source adjustment when playback stops
- Add native source-gain introspection and exercise it in the existing registered
  D1/D2 music integration script

Measurement at volume 8, first 120 seconds per selection (short CD tracks to end):

| Group                                                  | Before median LUFS | After median LUFS |
| ------------------------------------------------------ | -----------------: | ----------------: |
| Seven D2 MIDI selections                               |             -23.67 |            -21.67 |
| Eight Definitive Collection Europe Disc 2 audio tracks |             -9.495 |           -21.495 |
| Four D1 control selections                             |            -28.685 |           -28.685 |

Median D2/CD difference is 14.175 dB before and 0.175 dB after. The adjusted D2
sample has zero PCM boundary samples; its highest true peak is -0.82 dBFS
(game02). D1 game21 was separately stress-rendered with the proposed shared +2 dB
boost and produced 245 clipped samples, so D1 was excluded from the MIDI boost

The report and unnormalized listening excerpts are in
`android/temp/music-loudness-20261005/after/report.html`. Reproduction commands and
limitations are documented in `android/tests/music_spectral/README.md`. The host
comparison uses the production synth and source hashes, with FFmpeg CD reference
resampling; it does not claim exact device PCM or loudness equality for every song

The first emulator run caught the missing D2 definition on the SDL static library;
the Android CMake target now supplies it as well as the final game library

Final checks passed: Android x86_64 APK/CMake build for both engines, four host
synth/timeline/converter CTests, all 19 measured audio selections, D1 and D2
on-device music integration, both automation catalog checks, and scoped code
quality. D2 engine introspection confirmed the +2 dB source adjustment; D1
confirmed zero source adjustment. Existing unrelated working tree edits remain
