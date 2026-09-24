# Clean FM playback default

User selected clean resampling with higher precision, without warm filtering.

1. Implement a bounded streaming windowed-sinc converter matching the audition's
   Kaiser beta 8.6 / 94% Nyquist cutoff / 96-tap-per-phase design
2. Render ymfm at its native integer clock rate and unity gain, preserving float
   samples through double-precision filtering and gain before PCM16 conversion
3. Reset filter history with synth state, preserve MIDI event ordering and existing
   SoundFont behavior, and keep the original audition reproducible
4. Verify tone response, chunk invariance, reset/rate changes, comparison with the
   approved music, host integration, Android builds and device performance

Use a causal version of the audition filter with approximately 1 ms delay. No
new third-party dependency or warm filter. The existing SDL PCM16 music queue and
callback volume remain the output boundary; this task does not replace the whole
game audio mixer with floating point.

## Completed

- Shared production synth now uses native-rate unity-gain ymfm float output,
  streaming Kaiser-windowed sinc filtering with double accumulation, float output,
  double synth gain and one rounded/saturated conversion at the PCM16 queue
- Filter setup is outside rendering; fixed-size render batches reuse allocated
  storage, and filter/clock history resets with the synth
- No preference migration or new toggle: all existing AdLib/FM selections use
  clean output; SoundFont synthesis and callback volume/mixer remain unchanged
- Approved game01/game07/game08 comparisons agree at 73.44/72.27/72.19 dB after
  the fixed 48-frame delay, without adjusting gain or fitting alignment
- All three register traces are unchanged; exact reset, full-song repeat, and
  partial-block seek reconstruction pass. Legacy audition renders retain their
  original SHA-256 hashes
- Tone/chunk/reset/rate-reassertion tests pass from 8 through 192 kHz; 24.6 kHz
  rejection at 48 kHz output is 88.48 dB
- Relevant host CTest suites and scoped formatting/lint pass
- Debug APK builds for arm64-v8a, armeabi-v7a and x86_64
- Android x86 native synth/resampler checks pass; game08 is byte-identical to the
  Windows output and renders 61 seconds in 1.58 seconds on the emulator
- Installed APK on emulator-5554 and passed music track-control automation for
  D1 (41/41 steps) and D2 (48/48), including launcher preview seeking for D2

Evidence: `temp/fm-clean-default/`, especially `shipping-report.json`, build logs,
per-track integration logs and device-d1/device-d2 logs. Performance on physical
ARM phones remains unmeasured; the APK includes both ARM builds.
