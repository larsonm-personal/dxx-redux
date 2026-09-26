# Offline music spectral comparison

Run from the repository root on Windows with Python 3.11 and the native host
extract-test CMake build configured. No Android device or audio loopback is needed

```powershell
python -m venv temp/music-spectral-venv
temp/music-spectral-venv/Scripts/python.exe -m pip install -r android/tests/music_spectral/requirements.txt
& C:/local/android-sdk/cmake/3.31.6/bin/cmake.exe --build android/build/host-extract-tests --config Release --target test_music_synth -j 4
temp/music-spectral-venv/Scripts/python.exe android/tests/test_music_spectra.py
temp/music-spectral-venv/Scripts/python.exe android/tests/compare_music_spectra.py --output temp/music-spectral --seconds 90
```

Open `temp/music-spectral/report.html`. Graphs are embedded in the HTML; keep the
adjacent WAV files for listening. PNG graphs, JSON provenance and CSV measurements
are also exported. The runner returns failure if no pairs pass alignment. Individual
failed pairs remain visible in the report and do not silently disappear

The explicit `sc55.json` manifest maps HMP songs to OGG files. Paths resolve from
the repository root; edit a copy of the manifest and use `--manifest` for another
asset installation. `--renderer` selects another freshly built production renderer
and `--limit 2` is useful for a smoke run. A normal rerun replaces the output files;
use the retention helper before selecting a new timestamped output directory

`--reuse-renders` reuses PCM only when the renderer, font, revision, settings,
HOG and saved PCM hashes match the prior report. Rebuild and render afresh after
changing shared renderer DLLs; the executable hash alone does not identify those

## Interpretation

The main comparison matches 250-2000 Hz power with one constant gain per pair,
then shows the remaining difference. Positive high-frequency differences indicate
more treble relative to the midrange, not merely louder playback. Summaries give
each accepted song equal weight; shading is the middle 50% of song curves

FFmpeg measures LUFS and true peak on aligned sections without applying its
normalization output. Listening WAVs use a constant gain derived from those
measurements and a common target no higher than -23 LUFS, reduced when necessary
to preserve peak headroom. Clips contain the first 30 seconds of the matched
section, so their short-term loudness can differ from the full-section measurement

Stereo powers are averaged independently to preserve antiphase content. Spectra
use 4096-sample Hann Welch windows, 75% overlap, and logarithmic band integration
at 48 kHz. Bins over 60 dB below either spectrum's strongest band are masked

Alignment is bounded to +/-8 seconds and checked across three sections. Acceptance
requires full-envelope correlation >=0.35, section correlations >=0.25 and residual
offset spread <=0.20 seconds. This is an automated screening rule, not proof of
identical arrangement. Uncertain pairs retain plots but are excluded from summaries

The analysis covers up to 90 seconds by default, not full songs or loop repeats.
These are cold-start host renders with production defaults, not device captures.
The reference family name does not establish original hardware, SoundFont identity,
lossless provenance or mastering. Codec cutoffs, different patches and effects can
all influence the result. No EQ is fitted or applied by this tool

## Shared measured EQ

After generating the baseline, fit the shared D1+D2 correction and publish its
parameters to the app:

```powershell
temp/music-spectral-venv/Scripts/python.exe android/tests/fit_music_eq.py --publish
# Rebuild test_music_synth after publishing the generated header
temp/music-spectral-venv/Scripts/python.exe android/tests/validate_music_eq.py
```

`temp/music-eq/report.html` compares Detail/Balanced/Broad smoothing and reports
leave-one-song-out validation. `temp/music-eq/native/report.html` contains the
actual corrected production renders, before/after graphs and listening clips

The fit uses the arithmetic mean of dB differences, with each of the 28 accepted
songs weighted equally (23 D1, 5 D2). Gaussian smoothing has 0.5/1/2 octave full
width at half maximum on log2 frequency. Five broad RBJ biquads approximate its
inverse over 60 Hz-12 kHz; the shelf response extends outside the fitted range.
The final preset uses all accepted songs. No per-song adjustment occurs at runtime

Published parameters and provenance live in `assets/music_eq_sc55.json`; native
parameters are generated into `shared/music_eq_presets.h`. Native playback checks
the loaded bank's byte count and FNV64 content fingerprint before enabling the
correction. The catalog uses the SHA-256 identity in `MusicEq.kt` for imported
copies of the same bank. Update that identity and UI attribution when calibrating
a different bank; never relabel an existing curve for another bank

The editor exposes Flat and Measured EQ, with selectable smoothing. Balanced is
the recommended measured setting; Flat remains the default. Settings persist
globally for launcher previews and the next D1/D2 launch, including SF2 fallback.
Changing the profile stops the preview using the existing replacement path.
FM, recorded tracks and effects audio are unchanged

Installed-app tests (debug APK, emulator with D1/D2 base data):

```powershell
python android/tests/test_music_eq_device.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/music-eq/device --game-launch
python android/tests/music_realtime/probe.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/music-eq/realtime --renderer sf2 --eq measured-sc55-balanced --seconds 35 --max-render-ratio .5
```

Native `music_synth_tests` checks the EQ against independently computed responses,
exact Flat PCM, stereo isolation, reset, sample-rate and chunk-size behavior. The
corpus validator checks Flat parity with pre-EQ D1/D2 renders and corrected peaks
