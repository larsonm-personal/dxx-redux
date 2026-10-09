# Movie audio normalization

- [x] Measure locally owned intro, briefing and flyout movie soundtracks and compare
      with the existing gameplay music/effects measurements
- [x] Add a measured fixed movie gain to music_playback_levels.h and apply it in
      the Android movie playback path, preserving desktop behavior and dynamics
- [x] Verify production playback, relevant builds and scoped code quality
- [x] Record measurements, reproduction steps and limitations

Use the existing source-family calibration approach, without per-movie leveling
or compression. Keep unrelated changes and outstanding_bugs.md intact

## Measurements and decision

Measured all 24 complete soundtracks in the local US retail `intro-h.mvl` and
`other-h.mvl`: intro, ending, seven planetary briefings and fifteen flyouts.
FFmpeg decodes to 48 kHz stereo, then the existing music measurement helper
reports integrated LUFS and true peak. This is a source comparison; production
decoder captures below separately verify the actual Android gain

| Category            | Count | Original LUFS range | Adjusted median LUFS | Highest adjusted dBTP |
| ------------------- | ----: | ------------------: | -------------------: | --------------------: |
| Intro               |     1 |              -15.22 |               -25.32 |                 -9.45 |
| Ending              |     1 |              -13.90 |               -24.00 |                 -9.22 |
| Planetary briefings |     7 |    -14.20 to -13.19 |               -23.73 |                 -8.99 |
| Flyouts             |    15 |    -14.24 to -13.31 |               -23.67 |                 -8.66 |

Movies previously bypassed calibration: their decoded buffers were copied into
the SDL post-mix output at full amplitude. All audible MVE classes share this
path. Robot briefing loops explicitly disable MVE sound. D1 has no MVE path

Use `AUDIO_MOVIE_VOLUME_SCALE = 0.5f`, multiplied once by the existing
`AUDIO_GAMEPLAY_HEADROOM_SCALE = 0.625f`, for a net 0.3125 (-10.10 dB).
Apply this to decoded PCM before conversion/resampling and queueing, under
`__ANDROID__`. Handle signed 16-bit and unsigned 8-bit samples around their
respective zero points. Both SDL_mixer and direct SDL movie output share it

This is a chosen balance, not a claim that movie scores and gameplay music are
identical signals. The prior calibrated CD/MP3 captures were around -28.7 LUFS,
the long loud MIDI example -25.7 LUFS, and effects-heavy combined captures
-18.3 to -21.6 LUFS. Movie averages around -24 LUFS sit between background
music and action while preserving the authored dynamics and audible dialogue.
Applying the CD gain blindly would put these movies around -31 to -33 LUFS

No compressor, limiter, per-movie gain, asset modification, slider change or
desktop gain change. Any clipping already present in a source cannot be repaired
by attenuation, and digital measurements do not establish perceived loudness on
every speaker

## Reproduction

Use the existing `temp/music-spectral-venv` environment described in
`android/tests/music_spectral/README.md`, from the repository root:

```powershell
temp/music-spectral-venv/Scripts/python.exe android/tests/measure_movie_loudness.py `
  --library 'game_data/CD images/Descent II (USA)/data_tracks/d2data/intro-h.mvl' `
  --library 'game_data/CD images/Descent II (USA)/data_tracks/d2data/other-h.mvl' `
  --output temp/movie-audio-normalization
```

The report records each movie hash, duration, source and adjusted measurements,
FFmpeg version and calibration header. Adjacent original/adjusted WAV excerpts
allow listening at their measured relative levels. The report has stable JSON
with no timestamp and preserves identical report bytes on repeat runs

## Production playback verification

Captured the Android OpenSL output on emulator-5582 with the existing
`capture_audio` automation control, before and after the change. Intro capture
is about 20 seconds; planetary briefing `pla.mve` and flyout `esa.mve` captures
are about 8 seconds each. Scripts assert the expected movie is playing; flyout
uses the real endlevel transition from level 1. These are excerpts, so their
loudness differs from the full-source table above

| Capture  | Before LUFS | After LUFS | After dBTP | Fitted gain dB |
| -------- | ----------: | ---------: | ---------: | -------------: |
| Intro    |      -18.06 |     -28.16 |     -14.17 |       -10.1060 |
| Briefing |      -12.46 |     -22.56 |      -9.81 |       -10.1048 |
| Flyout   |      -14.27 |     -24.38 |     -10.35 |       -10.1052 |

All pairs aligned without an offset, exceeded 0.9999998 waveform correlation,
and measured within 0.01 dB of the intended gain. Adjusted captures contain zero
PCM boundary samples. The slight extra attenuation is 16-bit integer truncation.
Each before/after automation run passed (10 intro, 17 briefing, 20 flyout steps)

Artifacts are in `temp/movie-audio-normalization`: `report.json` (sources),
`runtime-report.json`, raw before/after WAVs, capture JSON scripts, logs, and
`verify_runtime.py` (installs the built APK, repeats captures, aligns samples and
asserts gain/correlation/headroom). It needs the three saved baseline WAVs for
its comparison. Baselines came from the previously installed version 24480;
the modified debug APK is version 24490

Validation passed:

- Android D1/D2 x86_64 CMake compilation and debug APK assembly via
  `gradlew.bat :app:assembleDebug '-Pandroid.injected.build.abi=x86_64' --console=plain --offline`
- Windows D2 build via `run-windows-build.ps1 -Target d2`
- Scoped mixed-language formatting/lint and `git diff --check`

The ABI-injected Gradle build places its APK under
`android/app/build/intermediates/apk/debug/app-debug.apk` and needs `adb install -r -t`.
The conventional outputs directory contained an older separate-package APK;
the first comparison correctly rejected its unchanged gain. Final comparisons
used the newly built `com.dxxredux.app` APK. Build warnings were in unrelated
existing gauges/weapon code; none were introduced in the changed playback code
