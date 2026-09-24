# FluidSynth SoundFont quality experiment

This standalone runner retains the shipping HMP conversion and MIDI timeline,
then compares TinySoundFont with FluidSynth. The Android MIDI soundfont path now
uses FluidSynth with reverb and chorus enabled by default. The default AdLib
renderer remains unchanged. Exact SC-55 emulation is out of scope.

## Listening

Open `temp/fluidsynth-feasibility/listening/listen.html`. Choose game01, game07 or
game08, then the formerly bundled TimGM6mb bank or nitro-shoe SC-55 1.34. The five variants
are current TSF, FluidSynth dry, reverb only, chorus only, and both effects.
Switching variants retains playback position. Start with current versus dry;
then compare dry versus reverb and chorus separately.

Each clip is 60 seconds. The same exported MIDI is passed to both renderers;
both use 48 voices and 48 kHz output. FluidSynth uses seventh-order interpolation,
double-precision DSP and a single synthesis thread. Its gain is 0.2; TSF retains
the shipping -10 dB gain. Listening copies are independently RMS-matched with a
peak cap. They are not BS.1770 loudness-matched; raw copies and gains are saved.
Existing PCM clipping in a baseline cannot be undone by normalization and is
recorded as `pcm_boundary_samples`. FluidSynth renders must not clip.

Effects are synthesizer effects, not a filter added to the finished recording:

- Reverb: FDN engine, room size 0.2, damping 0, width 0.5, level 0.3. Preset/default
  and song effect sends are respected. These parameters are experimental
- Chorus: three voices, level 1, speed 0.3 Hz, depth 8 ms. The chorus variants
  explicitly add CC93=24 to melodic channels; drums keep their original sends.
  Later MIDI commands can override this. Enabling an effect does not otherwise
  guarantee an audible effect when the song/bank sends nothing to it

The two FluidSynth effects variants must not be described as an authentic SC-55
profile. This experiment tests soundfont support and user preference.

## Build and reproduce on Windows

Use CMake 3.24+ and an installed MSVC toolchain. From the repository root:

```powershell
cmake -S android/tests/fluidsynth_quality -B temp/fluidsynth-feasibility/host -A x64
cmake --build temp/fluidsynth-feasibility/host --config Release --target fluid_render --parallel 4
ctest --test-dir temp/fluidsynth-feasibility/host -C Release -R '^fluid_render_contracts$' --output-on-failure
cmake --build android/build/host-extract-tests --config Release --target hmp_midi_export midi_tsf_render --parallel 4
python -m pip install numpy==2.2.6 soundfile==0.13.1
python android/tests/fluidsynth_quality/experiment.py --fluid temp/fluidsynth-feasibility/host/bin/Release/fluid_render.exe --hog path/to/descent.hog --font 'TimGM6mb=path/to/TimGM6mb.sf2' --font 'Nitro-shoe SC-55 1.34=temp/sc55-validation/Roland.SC-55.sf2' --output temp/fluidsynth-feasibility/listening
```

Use an isolated Python environment. `--font Label=path` can be repeated for other
banks. `assets/gm.sf2` now contains SC-55 1.34; download TimGM6mb separately
to reproduce the original two-bank comparison. The runner intentionally uses Windows host executable names. Do not rebuild
the host DLL while its listening renders are running. Invoke the repository's
artifact retention helper before creating a new output generation.

## Android feasibility

The same project cross-compiles with the NDK. Example (substitute local tool paths):

```powershell
cmake -S android/tests/fluidsynth_quality -B temp/fluidsynth-feasibility/android-x86_64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/local/android-ndk-r30/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release
cmake --build temp/fluidsynth-feasibility/android-x86_64 --target fluid_render --parallel 4
python android/tests/fluidsynth_quality/android_probe.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --build temp/fluidsynth-feasibility/android-x86_64 --ndk C:/local/android-ndk-r30 --font temp/sc55-validation/Roland.SC-55.sf2 --midi temp/sc55-validation/coverage/d1/game07.hmp.mid --output temp/fluidsynth-feasibility/device
```

Repeat the build with arm64-v8a and armeabi-v7a for the app's other ABIs. The probe
uses only `/data/local/tmp/dxx-fluidsynth-quality`; it does not alter game settings
or launch the app. It checks buffer-size invariance, audible effects, HMI bank/pan
retention, hard-reset silence, and wet seek/loop reconstruction against uninterrupted
rendering. It then renders 15 seconds dry and wet and saves performance data.

An observed FluidSynth 2.6.1 reset issue after controller modulation is handled by
issuing `fluid_synth_all_sounds_off(..., -1)` after `fluid_synth_system_reset()`.
Tests verify silence and zero active voices; they do not hide the issue by waiting
for a longer release timeout. A small already-rendered output block may remain.

Offline emulator throughput is not an in-game underrun test or a phone benchmark.
The production adapter in `shared/music_fluid.cpp` loads bounded, validated SF2
snapshots, including APK assets. It preserves HMI program/bank/pan and uses the
existing worker/seek/cancellation path. It supports the library's 8-96 kHz range.
Effects are independent game preferences, exported with settings and restored to
on by both reset presets. Both resets still select AdLib and the bundled font.

Production reset replaces DSP state and transfers the loaded bank after old
voices are destroyed. This clears chorus phase and partially buffered audio,
without rereading or decoding samples. The production adapter's exact PCM tests
cover repeated reset, wet seek and loop reconstruction at 44.1 and 48 kHz.
`test_music_synth` is the shared adapter/converter/timeline integration test.

Installed-app coverage (backs up/restores settings and its imported fixture):

```powershell
python android/tests/test_soundfont_profiles.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/fluidsynth-integration/app --games
```

This passes import/rejection, selection/effect persistence, both reset presets,
preview switching and D1/D2 music controls. Real-phone listening/performance
remains to be checked. The app retains raw headroom; the audition page's
per-clip RMS normalization is not applied to live playback.

## Production gain calibration

Production FluidSynth gain is now 0.4 at the app's default -10 dB setting,
up from 0.2 (+6.02 dB). One shared constant applies to gameplay, previews and
FM fallback to soundfont playback. FM gain and user volume settings are unchanged.
This is a fixed calibration; individual songs and soundfonts are not normalized.
The historical audition runner and recordings above retain their original 0.2 gain.

Raw 60-second comparisons of D1 game01/game07/game08 and D2 descent/game01/game02
are saved in `temp/midi-gain-calibration`. The median MIDI-to-FM RMS gap before
the adjustment was 5.46 dB with the bundled SC-55 bank. RMS is a useful level
check here, not a perceptual loudness match between different arrangements.
All six rebuilt samples increased by 6.02 dB without clipping; the highest
peak was -3.17 dBFS. This measures the bundled bank and these passages only.

## Dependencies and licensing

- FluidSynth **2.6.1**, LGPL-2.1-or-later, built as a separate shared library
- GCEM commit **012ae73c6d0a2cb09ffe86475f5c6fba3926e200**, Apache-2.0,
  compile-time math headers required by this FluidSynth release
- Existing pinned TinyMidiLoader, MIT, for identical MIDI event parsing

Versions, source URLs and SHA-256 values live in `android/get_deps/tool_versions.conf`;
`cmake/fluidsynth-music.cmake` reads those pins for every build. GLib is not needed with this
release's C++ platform layer. Audio drivers, networking, readline (GPL), OpenMP,
LADSPA, libsndfile, DLS and optional Signalsmith processing are disabled. No GPL
dependency is enabled. Soundfont licenses remain separate from library licenses.

The CMake file makes one numeric conversion explicit for Clang, preserving its
original behavior. It verifies the source text and skips already-applied edits.
MSVC warning C5287 about upstream's mixed enum bitmasks is disabled only for the
third-party library target; adding integer casts did not silence that diagnostic.
Upstream copyright/license headers remain intact. Full license texts, authors
and a provenance notice are included in the APK's `assets/licenses`. Publish the
corresponding-source ZIP alongside distributed builds containing this library:

```powershell
python android/tests/fluidsynth_quality/package_sources.py --fluid-source temp/fluidsynth-feasibility/host/_deps/fluid-src --gcem-source temp/fluidsynth-feasibility/host/_deps/gcem-src --output temp/fluidsynth-integration/fluidsynth-2.6.1-dxx-sources.zip
```

Use source directories from the exact configured build being distributed. The
ZIP includes the modified library, GCEM, notices and an offline CMake project
with desktop/Android rebuild instructions. It contains no game data.

## Updating FluidSynth

Run `android/get_deps/check-updates.ps1 -NoPrompt` to check for stable upstream
tags, or run it interactively and select FluidSynth to update. Builds never
select a floating latest version. The updater downloads and hashes the selected
FluidSynth archive, reads that tag's
[GCEM declaration](https://github.com/FluidSynth/fluidsynth/blob/v2.6.1/cmake_admin/FindGCEM.cmake),
then downloads GCEM and checks it against upstream's expected hash. All six pins
are replaced together after verification succeeds. Unsupported upstream metadata
or a failed download leaves the manifest unchanged. GCEM follows FluidSynth's
tested revision, rather than updating independently to GCEM's newest commit.

Before shipping an update:

1. Review upstream release notes, licenses, newly enabled dependencies and the
   local numeric-cast patch. Keep GPL components disabled. Refresh bundled notices
   and version references in this README as appropriate
2. Reconfigure/build the host renderer and run `fluid_render_contracts` plus
   `music_synth_tests`. Build all three Android ABIs and run the installed-app
   soundfont profile test above. Check game01, game07 and game08 by listening
3. Regenerate corresponding sources from the release build's actual source
   directories. The ZIP snapshots the relevant central pins and both CMake
   helpers. Extract it and verify an offline rebuild before publishing it beside
   the APK. Do not reuse an older source ZIP after updating the library

The updater refreshes pins; it does not establish playback quality or approve a
release. This centralization keeps the existing FluidSynth 2.6.1/GCEM versions.

## game01 high-note investigation

The original experiment's 48-voice cap is reached with SC-55 v1.34. Raising it
allows up to 67 simultaneous synthesized voices (including layers and release
tails) in the first 36 seconds. PCM changes around the reported 29-32 second
passage. Neither render clips. Production defaults to 128 voices for MIDI;
this removes that observed ceiling without altering FM voice allocation.

This is evidence of voice stealing, not proof that it explains the perceived
popping. The user reported no audible difference between the focused examples;
the popping remains unresolved. Remaining timbre/sample differences
have not been corrected with filters or soundfont-specific instrument edits.
The high sustained part is channel 5, program 51 (zero-based), Syn.Strings2.

```powershell
python android/tests/fluidsynth_quality/popping.py --fluid temp/fluidsynth-feasibility/host/bin/Release/fluid_render.exe --font temp/sc55-validation/Roland.SC-55.sf2 --midi temp/fluidsynth-feasibility/listening/raw/game01.mid --output temp/fluidsynth-integration/popping
```

Before the production gain calibration above, the adapter's 36-second game01 output was byte-for-byte identical to
this 128-voice reverb+chorus render. The diagnostic uses the same gain for all
versions and also provides the isolated high part; no normalization or filtering.
