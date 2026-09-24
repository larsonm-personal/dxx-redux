# FluidSynth SoundFont support

User direction: improve SoundFont synthesis and reverb/chorus support, retaining
the MIDI pipeline. Exact SC-55 emulation and ROM-based renderers are out of scope.
MIT/BSD preferred, LGPL accepted, GPL dependencies prohibited.

1. Pin an official FluidSynth release and inspect its library/dependency licenses
2. Render game01, game07 and game08 with identical MIDI and SoundFonts through
   current TSF and FluidSynth, isolating dry synthesis, reverb and chorus
3. Verify Android library compatibility, rendering performance and the shared
   playback adapter's reset, loop, pause and seek requirements
4. Save reproducible tooling and a listening page, then select effect defaults
   using comparisons rather than assuming the loudest/wettest result is best

Keep unrelated pending MIDI status-label changes intact. Preserve the AdLib/FM
default and its existing synthesis path.

## Experiment results (2026-09-23)

- Pinned FluidSynth 2.6.1 source SHA-256 and its Apache-2.0 GCEM math dependency
- Built a shared synthesis-only library with C++ platform support: no GLib,
  readline, external drivers, networking, OpenMP or optional DSP libraries
- Added `android/tests/fluidsynth_quality/` with CMake, native contracts, a
  repeatable audition runner and an Android native probe
- Produced 30 complete 60-second clips: three D1 songs, two fonts, five variants
  (TSF, FluidSynth dry, reverb, chorus, both). All effects variants differ from
  their dry counterpart, and no FluidSynth clip hits the PCM boundary/clips
- Current TSF baselines hit the PCM boundary in a few samples (2 in bundled
  game01; 32 in bundled game08; 12 in SC-55 game08). These remain in raw baselines;
  level matching cannot repair them. Renderer gains differ, so this is not a
  claim that FluidSynth inherently prevents clipping
- Built Android x86_64, arm64-v8a and armeabi-v7a. The Android x86_64 native probe
  passes buffer-size, effects, controller retention, reset, wet seek and loop checks
- The same native contracts pass in the Windows Release build. Scoped quality
  checks passed, supplemented by direct clang-format/cmake-format/cmake-lint for
  the experiment folder (outside the shared helper's native/CMake discovery scope)
- On emulator-5554, 15 seconds of game07/SC-55 took 2.262 seconds dry and 2.223
  seconds with effects. Scheduler outliers remain (36-48 ms worst calls); this
  is not evidence of phone realtime safety or in-game underrun performance
- Fixed the experiment's reset sequence: system reset after controller modulation
  left a voice active; an explicit all-sounds-off after reset clears it. Tests
  require zero active voices and silence without extending a release timeout

Listening page: `temp/fluidsynth-feasibility/listening/listen.html`
Measurements and provenance: `listening/report.json` and per-render logs
Android contracts/benchmark: `temp/fluidsynth-feasibility/device/report.json`

## Production integration (2026-09-23)

User selected reverb + chorus. Integrated FluidSynth behind the existing MIDI
soundfont option; AdLib remains the primary default and is unchanged. Effects
are independent saved switches, exported/imported with game preferences, and
both reset presets restore wet MIDI defaults along with AdLib and bundled SF2.

- Shared pinned CMake library for experiments, host tests and the Android app
- Bounded validated custom/APK SF2 snapshots; no disk I/O on render callbacks
- HMI channel-state retention and default chorus sends match the experiment
- Deterministic wet reset/seek by replacing DSP state and transferring the bank
- 128 MIDI voices: game01/SC-55 reaches 67, above the old 48-voice ceiling
- Full LGPL/Apache notices and corresponding-source packaging/rebuild script
- D1/D2 integration, preview switching, invalid import, preference restart and
  both settings resets pass on emulator-5554
- All three Android ABIs build; 1,074 JVM tests pass, one skipped; shared-native
  integration and 15 preview synchronization tests pass
- Production game01 PCM exactly matches the 128-voice wet experiment

The reported harsh popping around 30 seconds is not confirmed fixed. Raising
polyphony changes that passage and avoids the observed voice ceiling, but
subjective confirmation is still needed. The focused comparison and isolated
Syn.Strings2 part are in `temp/fluidsynth-integration/popping/listen.html`.
Real-phone performance/listening remains outside the emulator checks.

## Central dependency maintenance (2026-09-23)

The user heard no difference between the focused popping examples. Retain the
voice headroom, but do not describe it as a confirmed audible fix.

Move FluidSynth version, GCEM commit, both archive URLs and both SHA-256 pins to
`android/get_deps/tool_versions.conf`, retaining the existing versions. Extend
the update checker to follow stable FluidSynth tags and that release's GCEM pin,
verify both downloads before replacing the pins together, and preserve offline
corresponding-source packaging. Verify failure paths, native rendering contracts
and an extracted offline source rebuild. Document the review/build/listening
checks required before shipping future dependency updates.

Completed with the existing pins unchanged. Verified the updater's successful
six-field replacement and unchanged manifests after interrupted downloads,
hash mismatches and unsupported metadata. A live upstream download reproduced
the existing pins exactly. Three dependency-verification tests, both native
rendering contracts and scoped quality checks pass. The regenerated source ZIP
builds from a fresh extraction with FetchContent disconnected and HTTP(S) proxies
pointing to an unavailable local endpoint. No APK rebuild was needed for this
dependency-maintenance change.
