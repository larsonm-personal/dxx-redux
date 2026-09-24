# FM output-quality audition

Open `temp/fm-quality/listening/listen.html` after running the experiment. It has
60-second excerpts of game01, game07 and game08, with position-preserving switching.
All use the shipping HMP/HMQ selection, original AdLib bank, HMI driver patches,
OPL3 mode and ymfm chip clock. Running the experiment does not change app settings.

Clean resampling with higher precision is now the production FM default. The
experiment retains the original **Current** output as a legacy comparison via
guarded source transformations; **Warm** remains experiment-only.

## Versions

- **Current**: pre-upgrade renderer, including integer intermediate rounding and
  weighted sample-averaging rate conversion
- **More precision**: identical rate conversion, with double intermediates and a
  float outlet so fractional precision survives mixing and the -10 dB synth gain
- **Clean resampling**: native 49,715 Hz chip output at unity gain, followed by
  offline windowed-sinc conversion to 48 kHz and the same -10 dB gain
- **Warm: two poles**: original warm version, clean resampling followed by a one-pole 5 Hz high-pass and
  two-pole 8 kHz Butterworth low-pass; a tonal experiment, not a measured card model
- **Warm: one pole**: same clean source, DC removal and 8 kHz cutoff, changing only
  the low-pass order to one; it attenuates less above the cutoff but more below it

The page includes shortcuts to game01 at 0:30 and game07 at 0:45. Compare both warm
versions with Clean resampling, especially on game08: fewer poles does not make
filtering neutral. Both versions are about -3 dB at 8 kHz before level matching.
The report includes filter responses at several frequencies, before presentation
gain. These are output filters applied after synthesis and resampling.

The chip core still produces its authentic quantized output. Extra processing
precision avoids additional rounding; it does not restore bits the chip never
generated. Sinc conversion can reduce conversion artifacts, but cannot remove
aliasing already present in the chip's native output. Its filter uses a Kaiser
window (beta 8.6), 22,560 Hz cutoff and compensated delay. The streaming production
implementation uses the same cutoff/window/support, with an interpolated phase
table, double accumulation and a causal delay (48 output frames at 48 kHz).
It retains floating-point native output through filter and synth gain, then
rounds/clamps once to the existing PCM16 music queue. The game's subsequent
callback volume and SDL mixing remain PCM16.

## Fair comparison and checks

Listening WAVs are all stereo 48 kHz PCM16 with deterministic TPDF dither at final
conversion. Each receives constant gain to a shared gated K-weighted loudness
target, at most -20 LUFS, lowered if necessary for peak headroom. No compression
or limiter is used. These are loudness/oversampled-peak estimates, not certified
meter measurements. The current version receives this presentation gain too;
its untouched production render remains in `raw/`.

The runner verifies actual FM selection and the selected arrangement (HMQ for
game01/game08, HMP for game07), identical bank selection, equal ordered
register events at millisecond resolution, byte-exact repeatability of baseline
and native rendering, duration, finite samples, loudness matching and clipping.
Native-rate event quantization differs by at most a sample; native renders include
one extra second to avoid an artificial resampling boundary at the listening cut.
It tests the sinc filter with 1 kHz, 20 kHz and above-output-Nyquist tones. The JSON
report includes results, audio differences, input hashes and source hashes.

## Reproduce on Windows

Requires Python 3.11, CMake and Visual Studio 2022 C++ tools. From the repository:

```powershell
.\android\tests\fm_quality\run.ps1 -Hog 'path\to\descent.hog'
```

Optional parameters: `-Seconds 60` (10-119), `-Work temp/fm-quality`, and `-Output`
for a different listening directory. The runner uses a local virtual environment
with NumPy 2.2.6 and SciPy 1.15.3, and the repository's exact pinned ymfm,
ymfmidi and TinySoundFont revisions. The shipping harness loads the bundled SF2
because its fallback interface requires it, but the runner rejects any render
that actually falls back to SoundFont playback. The source HOG is user-supplied.

`prepare.py` makes guarded copies of the production harness/player in the build
directory; it fails if expected source fragments change. Rerunning overwrites
the experiment's fixed-name output files. The native/float executables are
experiment-only `--render` tools, not substitutes for the production test suite.

Dependencies: [ymfm (BSD-3-Clause)](https://github.com/aaronsgiles/ymfm),
[ymfmidi (BSD-3-Clause)](https://github.com/devinacker/ymfmidi),
[TinySoundFont (MIT)](https://github.com/schellingb/TinySoundFont),
[NumPy (BSD-3-Clause)](https://numpy.org/doc/stable/license.html), and
[SciPy (BSD-3-Clause)](https://github.com/scipy/scipy/blob/v1.15.3/LICENSE.txt).
No new playback dependency or instrument bank is added to the app.

## Verify the production default against the approved audition

The CMake project also builds `fm_quality_shipping` and `test_fm_resampler`.
After building, run CTest and compare with an existing clean audition:

```powershell
ctest --test-dir temp/fm-quality/build -C Release --output-on-failure
temp/fm-quality/venv/Scripts/python.exe android/tests/fm_quality/verify_shipping.py --build temp/fm-quality/build/Release --reference temp/fm-quality/listening --output temp/fm-clean-default
```

The comparison checks all register events, applies the fixed 48-frame causal
delay, and requires over 65 dB signal-to-error ratio without fitting timing or
gain. Resampler tests cover tone response, 8-192 kHz output, exact reset/chunk
invariance and reasserting the output rate without interrupting playback.
