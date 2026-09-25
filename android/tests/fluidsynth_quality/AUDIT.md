# FluidSynth integration audit

Reviewed against pinned FluidSynth 2.6.1 source on 2026-09-23. Scope: the shared
production adapter, its gameplay/preview callers, CMake integration, actual
Android Debug compiler commands, and historical offline experiments

## Finding and fix

The deprecated `FLUID_INTERP_7THORDER` has value 7, which now selects
`FLUID_INTERP_HIGHEST`: 25-point sinc. The pinned implementation dispatches to
`InterpolateSinc<25>`. It is no longer seven-point interpolation. Upstream's
[API documentation](https://www.fluidsynth.org/wiki/api/synthesis-params/)
explicitly warns against this mode for real-time rendering

Production now selects `FLUID_INTERP_4THORDER` explicitly after both initial
loading and DSP recreation, checking the return value. This changes sample
interpolation; it does not change notes, voice limits, bank, effects, gain or
sample rate. Listening is still necessary to judge the audible difference

The existing host synth integration test now compares production PCM with an
explicit fourth-order reference using a pitch-bent layered instrument. It checks
initial load, reset and a 48-to-44.1 kHz rate change, with effects enabled

Historical experiments retain their 25-point sinc default for reproducibility,
using the unambiguous `FLUID_INTERP_HIGHEST` name. Their documentation and generated
listening-page text now identify the actual interpolation. Passing `128 -1 4`
to `fluid_render` selects production polyphony and interpolation; its historical
0.2 gain still differs from production's calibrated 0.4

## Runtime settings

| Setting | Current choice | Audit outcome |
| --- | --- | --- |
| Sample rate | Initially 48 kHz; gameplay adopts mixer rate; accepted SF2 range 8-96 kHz | Matches upstream range. Rate changes recreate the synth; avoids upstream's obsolete no-op sample-rate setter |
| Gain | 0.4 at UI -10 dB; exponential relative adjustment | Deliberate +6.02 dB over upstream 0.2 default. PCM clamps at the queue boundary; no automatic normalization or limiter |
| Polyphony | 128; diagnostic adjustment bounded to 8-256 | Deliberate. SC-55 game01 reaches 67 voices, so returning to the old 48 cap would steal voices |
| CPU cores | 1 | Upstream default. Avoids extra workers competing with gameplay. No evidence that parallel rendering is needed after the interpolation fix |
| Thread-safe API | Off | Deliberate, conditional on exclusive synth ownership. Gameplay sends live tuning to the render worker and joins it before teardown. Preview serializes access with its playback mutex |
| Dynamic sample loading | Off | Upstream default; samples loaded before playback, avoiding disk I/O on note starts |
| Bank selection | GS | Upstream default, compatible with selected SC-55 bank and current HMI bank handling |
| Reverb | FDN; room 0.2, damp 0, width 0.5, level 0.3 | Deliberate listening profile. 2.6.1 defaults are DAT, 0.5, 0.2, 0.8, 0.7 respectively. No claim of authentic SC-55 DSP |
| Chorus | 3 voices, level 1, speed 0.3 Hz, depth 8 ms | Deliberate listening profile. Upstream defaults are 3, 0.6, 0.2 Hz, 4.25 ms. Waveform remains upstream sine default |
| Effect sends | CC93=24 on melodic channels when chorus enabled; drums retain their sends | Deliberate extra chorus send. MIDI can subsequently override it. Preset/song reverb sends remain in use |
| Portamento/minimum note duration/overflow policy | Upstream defaults | No overrides. Portamento `auto` preserves the upstream XG/GS interpretation unless MIDI specifies CC37; minimum note length remains 10 ms |

No further runtime setting changes are justified by the current evidence

## Rendering, reset and data handling

- Float output is converted to saturated PCM16 with rounding and nonfinite checks
  in blocks of up to 256 frames. This is additional adapter work but not the
  measured 25-point interpolation cost. No global fast-math is applied to this code
- Reset recreates DSP and transfers the loaded soundfont. This clears chorus phase
  and partial buffers for deterministic seeks, without rereading samples. It is
  preparation/seek work, not performed for every render block
- On failed recreation, reset falls back to system reset plus all-sounds-off.
  Allocation-failure behavior is a degraded fallback, not a claim of exact seek
  reconstruction under memory exhaustion
- HMI CC121 handling preserves bank and pan and resets sustain/pitch bend and
  volume LSB explicitly. Initial HMP channel volume is zero until authored events
  set it. These are intentional format adaptations, not performance workarounds
- Custom loader callbacks read a bounded, validated in-memory SF2 snapshot with a
  unique cache identity. Loading is synchronous; the temporary loader context is
  thread-local and samples are loaded eagerly
- Android synthesis runs on a producer worker with a 262144-sample stereo queue
  (about 2.73 seconds at 48 kHz). The mixer callback consumes PCM; it does not run
  FluidSynth. Neither worker explicitly requests elevated scheduling priority
- The large queue cannot fix sustained throughput below real time. Raising worker
  priority would not reduce CPU work. Neither was changed
- Existing callback breadcrumbs and throttled underrun logging can add callback
  overhead. This audit did not profile that overhead and does not attribute the
  reported hosting problem to it

## Build configuration and dependencies

Reviewed `cmake/fluidsynth-music.cmake`, the app CMake/Gradle files, upstream
`CMakeLists.txt`/`src/CMakeLists.txt`, generated configuration headers and actual
compiler commands for arm64-v8a, armeabi-v7a and x86_64

| Choice | Evidence and decision |
| --- | --- |
| Version pins | FluidSynth 2.6.1 and upstream's GCEM commit use exact URLs and SHA-256 hashes. TLS verification is on |
| Shared library | Intentional shared `libfluidsynth`; existing source-package procedure includes the exact patched source and pins |
| DSP precision | `enable-floats=OFF`: upstream default double precision, despite using the float output API. Kept; no evidence requiring a precision change |
| Debug/internal optimization | Actual Debug commands have `-O2` on adapter and FluidSynth object target for all three ABIs. Debug symbols/assertions retained. Internal uses CMake Debug |
| Release optimization | No local override of release optimization. Gradle's existing RelWithDebInfo configuration uses `-O2 -g -DNDEBUG`; standalone CMake Release follows toolchain defaults. This audit's rebuilt APK is Debug |
| Fast-math | No integration-wide fast-math. Upstream applies `-fno-math-errno -ffast-math` only to `fluid_iir_filter_impl.cpp` on Clang/GCC, `/fp:fast` on MSVC. Confirmed in Android commands; retained |
| Architecture | NDK ABI targets; ARMv7 uses `-march=armv7-a -mthumb`. No host-native CPU tuning or added ISA requirement |
| Threads | `enable-threads=ON`, C++11 OS abstraction, no GLib. One synthesis core selected at runtime; build-time thread support does not itself select parallel voice rendering |
| OpenMP | Off. Avoids an extra runtime and upstream parallel decoding/mixing paths; kept after the measured interpolation gain |
| Optional DSP | Signalsmith off, which also removes its limiter and alternate reverb. Production uses FDN and explicit PCM saturation |
| Audio/MIDI drivers | FluidSynth drivers, network, SDL3, OpenSL ES, Oboe, etc. disabled. App owns audio output, event scheduling and MIDI conversion |
| Formats/tools | libsndfile/SF3 support, native DLS, LADSPA, readline and platform service integrations disabled. This integration admits SF2 and uses its own PCM decoder |
| Diagnostic build switches | Generated cache has profiling, coverage, FPE checking/trapping and sanitizers off |
| Warning policy | `/wd5287` limited to upstream MSVC enum warnings; app warning policy remains in effect on Android |
| Source patch | Comparison with the unpacked 2.6.1 tree found only `src/drivers/fluid_audio_convert.h`: explicit float cast of a numeric limit. No synthesis algorithm patch |

Raw Debug commands and feature settings are recorded in
`temp/fluidsynth-audit/compile-flags.json`

## Validation

Two Windows Release offline comparisons of 45 seconds of SC-55 game01, effects
on and 128 voices, measured 13.611/13.391 seconds for 25-point sinc versus
0.396/0.398 seconds for fourth-order. Both reached 67 voices without clipped
samples. These are elapsed offline timings, not phone CPU percentages

Host `music_synth_tests`, `music_soundfont_tests`, `midi_seek_timeline_tests` and
`hmp_android_shared_tests` pass, as does the standalone `fluid_render_contracts`
test. Scoped formatting/lint and Python compilation pass. Android Debug native
builds passed for all three ABIs

An initial Gradle attempt encountered a locked Kotlin output directory. The
retry completed, but concurrent packaging produced an APK missing primary DEX.
After repackaging, a stable snapshot was checked for primary DEX and full ZIP
integrity and installed successfully on emulator-5556. Its checksum/ABI inventory
is in `temp/fluidsynth-audit/apk.json`

The installed-app probe has not established a new real-time performance result:
the emulators exhibited system-wide multi-second scheduling delays and launcher
introspection timed out after 60 seconds before playback began. The probe's
cleanup restored preferences and the timing property. Logs are in
`temp/fluidsynth-audit/preview-fixed.log`. The earlier Windows speedup is not
a substitute for phone or hosting-gameplay validation

The sustained probe now records elapsed render seconds per produced audio second
and accepts an optional `--max-render-ratio` budget. This includes scheduling
delays and is explicitly not a measurement of thread CPU utilization
