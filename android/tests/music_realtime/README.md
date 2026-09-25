# Installed-app music timing

This probe runs the launcher's actual native MIDI preview, using imported game
data and the bundled SC-55 bank with reverb and chorus enabled. It measures
audio consumption, queue starvation, render time and first nonzero PCM. It
restores game preferences and the diagnostic property after the run.

Build/install a debug APK, then run from the repository root:

```powershell
./android/helpers/retain-recent-artifacts.ps1 -Artifacts temp/music-realtime/check
python android/tests/music_realtime/probe.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/music-realtime/check --seconds 45
```

Use `--renderer sf2` or `--renderer ymfm` to isolate a renderer. D1 is selected
by source ID, not catalog index. `--source d2-builtin` tests the D2 songs.
Use `--measure-only` to retain a known-bad baseline without a failing exit code.
Use `--max-render-ratio 0.15` to require at most 0.15 elapsed render seconds per
produced audio second, excluding initial queue priming. Reports always include
this ratio; it includes scheduling delays and is not thread CPU utilization.
Run without concurrent builds or other device tests to avoid CPU contention.

The default cases are FM menu, briefing and game01, plus MIDI game01. A pass
requires ongoing consumption near 48 kHz, at most two underruns after the first
progress report, first nonzero PCM within 1.5 seconds, and progress reports
covering the requested duration. The minimum 35-second run reaches game01's
dense section; a short smoke test misses the original failure. Initial output
buffer priming is excluded from the sustained-underrun count. Reports and raw
logs are saved even when assertions fail.

`command_reply_ms` includes Android broadcast dispatch and is **not** synth
startup latency. `first_audio_ms` starts inside native preview start, after
stopping the previous song; it excludes soundfont initialization and includes
authored silence. It measures nonzero PCM entering the output queue, not
speaker latency. The reported preview position is the producer position and
can lead audible playback by the queued frames.

For opt-in diagnostics during manual gameplay or preview:

```powershell
adb shell setprop debug.dxx.music_timing 1
# Start a new song, then capture the profiling log
adb logcat -d -s DXX-DLOG:D '*:S'
adb shell setprop debug.dxx.music_timing 0
```

The property is read at song start. Gameplay reports stop/preparation time and
first nonzero PCM separately. Preview reports render/callback progress once per
second through the central profiling logger. No new logging happens inside an
audio callback. Diagnostics are off by default.

## September 23 baseline and optimization

On the x86_64 emulator, Android Debug originally compiled the shared renderer,
ymfm/player and FluidSynth without optimization. In a 35-second SC-55 game01
run, rendering fell below real time as voices accumulated, draining the queue
and producing 222 sustained underruns. Average audio consumption fell to
41,294 frames/second. With `-O2` on those targets, a 45-second repeat sustained
47,984 frames/second with zero sustained underruns. FM game01 rendering dropped
from roughly 200-300 ms to 30-40 ms per second of playback.

Debug symbols and assertions remain enabled. Double precision, the then-selected
SF2 interpolation, FM clean resampling, effects, voice limit and queue sizes
are unchanged. Release builds retain their existing optimization settings.

That baseline still selected deprecated `FLUID_INTERP_7THORDER`, which maps to
25-point sinc in FluidSynth 2.6.1. The subsequent fix selects fourth-order in
production; see [the FluidSynth audit](../fluidsynth_quality/AUDIT.md) for results.

The preview did not reproduce a 2-3 second FM startup delay: first nonzero PCM
was approximately 0.95 seconds for the menu, 61 ms for briefing and 446 ms for
game01 after optimization, including each song's initial silence. These are
emulator results, not confirmation of startup behavior on the user's phone.

Gameplay then exposed a separate HMP conversion cost: the game's debug
allocator copies on every reallocation. Appending a few bytes per MIDI event
made conversion quadratic. A 40 KB synthetic MIDI required 30,010 reallocations
totaling 601 MB of allocation requests. Geometric capacity growth fixes this
without changing MIDI contents; all 156 exports across 52 D1/D2 HMP/HMQ files
(once/repeat/legacy) matched the prior converter byte-for-byte. The native
`hmp_android_shared_tests` regression bounds allocation work and exercises
allocation failures during large legacy, GM and FM conversions.

Gameplay measurements with the converter fix (same emulator, same optimized
synth APK configuration):

| Case | Preparation before | Preparation after | First PCM before / after |
| --- | ---: | ---: | ---: |
| D1 menu, cold synth | 755 ms | 260 ms | 1677 / 1192 ms |
| D1 briefing | 51 ms | 7 ms | 58 / 19 ms |
| D1 game01, initial level | 454 ms | 25 ms | 905 / 467 ms |
| D2 menu, cold synth | 974 ms | 240 ms | 1241 / 493 ms |
| D2 game02 | 1593-1609 ms | 75-85 ms | 1931-1935 / 413-415 ms |

The remaining gap between preparation and first PCM includes the music's
authored lead-in. Cold menu preparation also includes loading the fallback
soundfont. D1/D2 music-control tests pass all 41/48 steps respectively. Raw
before/after logs are in `temp/music-realtime/game-*-native.log`.

The final APK's opt-in preview run (`temp/music-realtime/verified/report.json`)
also passes all four cases, including 45 seconds each of FM and SC-55 game01,
with zero post-startup underruns and sustained 48 kHz consumption.
